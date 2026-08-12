/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */

#include "protocol-test-client.h"
#include "protocol-test-xdg-client.h"
#include "treeland-keyboard-state-notify-unstable-v1-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

/* --- watcher event tracking --- */

struct watcher_state {
    int current_state_count;
    int state_changed_count;
    uint32_t last_modifier;
    uint32_t last_state;
    uint32_t changed_modifier[2];
    uint32_t changed_state[2];
};

static void handle_current_state(void *data,
                                struct treeland_keyboard_state_watcher_v1 *watcher,
                                uint32_t modifier,
                                uint32_t state)
{
    (void)watcher;
    struct watcher_state *ws = data;
    ws->current_state_count++;
    ws->last_modifier = modifier;
    ws->last_state = state;
}

static void handle_state_changed(void *data,
                                 struct treeland_keyboard_state_watcher_v1 *watcher,
                                 uint32_t modifier,
                                 uint32_t state)
{
    (void)watcher;
    struct watcher_state *ws = data;
    if (ws->state_changed_count < 2) {
        ws->changed_modifier[ws->state_changed_count] = modifier;
        ws->changed_state[ws->state_changed_count] = state;
    }
    ws->state_changed_count++;
    ws->last_modifier = modifier;
    ws->last_state = state;
}

static const struct treeland_keyboard_state_watcher_v1_listener watcher_listener = {
    .current_state = handle_current_state,
    .state_changed = handle_state_changed,
};

/*
 * Inject a CAPS_LOCK modifier state via the virtual keyboard's modifiers request.
 * This bypasses the key-event path which requires a focused surface -- in the
 * headless test the virtual keyboard has no focus surface, so pressing a key
 * never triggers notify_modifiers.  Using zwp_virtual_keyboard_v1_modifiers
 * with depressed=0 and locked=CAPS_LOCK directly updates the keyboard state.
 */
static int send_caps_lock(struct zwp_virtual_keyboard_v1 *keyboard)
{
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = ctx ? xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    if (!keymap) {
        if (ctx) xkb_context_unref(ctx);
        return 0;
    }

    xkb_mod_index_t caps_idx = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
    xkb_mod_mask_t caps_mask = caps_idx == XKB_MOD_INVALID ? 0 : (xkb_mod_mask_t)(1U << caps_idx);

    /* Depress shift (depressed=0), latch=0, lock=CAPS_LOCK, effective=caps_mask */
    zwp_virtual_keyboard_v1_modifiers(keyboard, 0, 0, caps_mask, caps_mask);

    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
    return 1;
}

static int clear_caps_lock(struct zwp_virtual_keyboard_v1 *keyboard)
{
    zwp_virtual_keyboard_v1_modifiers(keyboard, 0, 0, 0, 0);
    return 1;
}

static int send_keymap(struct zwp_virtual_keyboard_v1 *keyboard)
{
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = context
        ? xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS)
        : NULL;
    char *keymap_text = keymap
        ? xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1)
        : NULL;
    if (!keymap_text)
        goto failed;

    char name[64];
    snprintf(name, sizeof(name), "/treeland_keyboard_state_keymap_%d", (int)getpid());
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    shm_unlink(name);
    if (fd < 0)
        goto failed;

    const size_t size = strlen(keymap_text) + 1;
    if (ftruncate(fd, (off_t)size) < 0 || write(fd, keymap_text, size) != (ssize_t)size) {
        close(fd);
        goto failed;
    }
    zwp_virtual_keyboard_v1_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, (uint32_t)size);
    close(fd);
    free(keymap_text);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    return 1;

failed:
    free(keymap_text);
    if (keymap)
        xkb_keymap_unref(keymap);
    if (context)
        xkb_context_unref(context);
    return 0;
}

/* --- test --- */

int protocol_test_run(const char *socket_name)
{
    struct protocol_test_connection connection;
    if (!protocol_test_connect(&connection, socket_name))
        return 1;

    /* Bind required globals */
    struct wl_seat *seat = protocol_test_bind(&connection, "wl_seat",
                                               &wl_seat_interface, 1);
    struct treeland_keyboard_state_notify_manager_v1 *manager =
        protocol_test_bind(&connection, "treeland_keyboard_state_notify_manager_v1",
                           &treeland_keyboard_state_notify_manager_v1_interface, 1);
    struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager =
        protocol_test_bind(&connection, "zwp_virtual_keyboard_manager_v1",
                           &zwp_virtual_keyboard_manager_v1_interface, 1);
    struct zwp_virtual_keyboard_v1 *virtual_keyboard = NULL;
    if (!seat || !manager || !virtual_keyboard_manager) {
        fprintf(stderr, "keyboard-state-notify: failed to bind globals\n");
        goto failed;
    }

    virtual_keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
        virtual_keyboard_manager, seat);
    if (!virtual_keyboard || !send_keymap(virtual_keyboard)
        || wl_display_roundtrip(connection.display) < 0) {
        fprintf(stderr, "keyboard-state-notify: failed to create virtual keyboard\n");
        goto failed;
    }

    /* ---- Test 1: watcher with null seat ---- */
    {
        struct watcher_state ws = {0};

        struct treeland_keyboard_state_watcher_v1 *watcher =
            treeland_keyboard_state_notify_manager_v1_get_keyboard_state_watcher(manager, NULL);
        if (!watcher) {
            fprintf(stderr, "keyboard-state-notify: get_keyboard_state_watcher(null seat) failed\n");
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_add_listener(watcher, &watcher_listener, &ws);

        /* Configure: watch caps_lock, notify on locked and unlocked */
        treeland_keyboard_state_watcher_v1_set_modifiers(watcher,
            TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK);
        treeland_keyboard_state_watcher_v1_set_flags(watcher,
            TREELAND_KEYBOARD_STATE_WATCHER_V1_WATCH_FLAG_LOCKED |
            TREELAND_KEYBOARD_STATE_WATCHER_V1_WATCH_FLAG_UNLOCKED);
        treeland_keyboard_state_watcher_v1_apply(watcher);
        wl_display_roundtrip(connection.display);

        /* The initial Caps Lock state may be unlocked, so no current_state is required. */
        if (ws.current_state_count > 0) {
            /* Verify the event refers to caps_lock (value 1) */
            if (ws.last_modifier !=
                    TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK) {
                fprintf(stderr,
                        "keyboard-state-notify: expected modifier=caps_lock (%u), got %u\n",
                        TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK,
                        ws.last_modifier);
                treeland_keyboard_state_watcher_v1_destroy(watcher);
                goto failed;
            }
            /* state must be a valid modifier_state value (0 or 1) */
            if (ws.last_state > 1) {
                fprintf(stderr,
                        "keyboard-state-notify: invalid modifier_state %u\n",
                        ws.last_state);
                treeland_keyboard_state_watcher_v1_destroy(watcher);
                goto failed;
            }
        }
        /* No state_changed events should have been sent */
        if (ws.state_changed_count != 0) {
            fprintf(stderr,
                    "keyboard-state-notify: unexpected state_changed events: %d\n",
                    ws.state_changed_count);
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_destroy(watcher);
    }

    /* ---- Test 2: watcher with a real seat, no modifiers set ---- */
    {
        struct watcher_state ws = {0};

        struct treeland_keyboard_state_watcher_v1 *watcher =
            treeland_keyboard_state_notify_manager_v1_get_keyboard_state_watcher(manager, seat);
        if (!watcher) {
            fprintf(stderr, "keyboard-state-notify: get_keyboard_state_watcher(seat) failed\n");
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_add_listener(watcher, &watcher_listener, &ws);

        /* Apply with no modifiers configured -- must not crash, no events expected */
        treeland_keyboard_state_watcher_v1_apply(watcher);
        wl_display_roundtrip(connection.display);

        if (ws.current_state_count != 0 || ws.state_changed_count != 0) {
            fprintf(stderr,
                    "keyboard-state-notify: unexpected events with no modifiers set "
                    "(current_state=%d, state_changed=%d)\n",
                    ws.current_state_count, ws.state_changed_count);
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_destroy(watcher);
    }

    /* ---- Test 3: a real virtual keyboard toggles Caps Lock on the real seat ---- */
    {
        struct watcher_state ws = {0};
        struct treeland_keyboard_state_watcher_v1 *watcher =
            treeland_keyboard_state_notify_manager_v1_get_keyboard_state_watcher(manager, seat);
        if (!watcher) {
            fprintf(stderr, "keyboard-state-notify: E2E watcher creation failed\n");
            goto failed;
        }
        treeland_keyboard_state_watcher_v1_add_listener(watcher, &watcher_listener, &ws);
        treeland_keyboard_state_watcher_v1_set_modifiers(
            watcher, TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK);
        treeland_keyboard_state_watcher_v1_set_flags(
            watcher, TREELAND_KEYBOARD_STATE_WATCHER_V1_WATCH_FLAG_LOCKED
                | TREELAND_KEYBOARD_STATE_WATCHER_V1_WATCH_FLAG_UNLOCKED);
        treeland_keyboard_state_watcher_v1_apply(watcher);
        if (wl_display_roundtrip(connection.display) < 0 || ws.state_changed_count != 0) {
            fprintf(stderr, "keyboard-state-notify: unexpected state before Caps Lock toggle\n");
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }

        /* Inject CAPS_LOCK via zwp_virtual_keyboard_v1_modifiers: no focused
         * surface is needed, unlike key events which require keyboard focus. */
        if (!send_caps_lock(virtual_keyboard)) {
            fprintf(stderr, "keyboard-state-notify: send_caps_lock failed\n");
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }
        if (wl_display_roundtrip(connection.display) < 0 || ws.state_changed_count != 1
            || ws.changed_modifier[0] != TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK
            || ws.changed_state[0] != TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_STATE_LOCKED) {
            fprintf(stderr, "keyboard-state-notify: missing Caps Lock locked event\n");
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }

        /* Toggle back to unlocked */
        if (!clear_caps_lock(virtual_keyboard)) {
            fprintf(stderr, "keyboard-state-notify: clear_caps_lock failed\n");
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }
        if (wl_display_roundtrip(connection.display) < 0 || ws.state_changed_count != 2
            || ws.changed_modifier[1] != TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK
            || ws.changed_state[1] != TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_STATE_UNLOCKED) {
            fprintf(stderr, "keyboard-state-notify: missing Caps Lock unlocked event\n");
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_destroy(watcher);
    }

    /* Cleanup */
    zwp_virtual_keyboard_v1_destroy(virtual_keyboard);
    zwp_virtual_keyboard_manager_v1_destroy(virtual_keyboard_manager);
    treeland_keyboard_state_notify_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    protocol_test_disconnect(&connection);
    return 0;

failed:
    if (virtual_keyboard) zwp_virtual_keyboard_v1_destroy(virtual_keyboard);
    if (virtual_keyboard_manager) zwp_virtual_keyboard_manager_v1_destroy(virtual_keyboard_manager);
    protocol_test_disconnect(&connection);
    return 1;
}
