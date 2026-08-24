// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "treeland-keyboard-state-notify-unstable-v1-client-protocol.h"

#include <stdio.h>

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

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = client_bind(&connection, "wl_seat",
                                               &wl_seat_interface, 1);
    struct treeland_keyboard_state_notify_manager_v1 *manager =
        client_bind(&connection, "treeland_keyboard_state_notify_manager_v1",
                           &treeland_keyboard_state_notify_manager_v1_interface, 1);
    if (!seat || !manager) {
        fprintf(stderr, "keyboard-state-notify: failed to bind globals\n");
        goto failed;
    }

    {
        struct watcher_state ws = {0};

        struct treeland_keyboard_state_watcher_v1 *watcher =
            treeland_keyboard_state_notify_manager_v1_get_keyboard_state_watcher(manager, NULL);
        if (!watcher) {
            fprintf(stderr, "keyboard-state-notify: get_keyboard_state_watcher(null seat) failed\n");
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_add_listener(watcher, &watcher_listener, &ws);

        treeland_keyboard_state_watcher_v1_set_modifiers(watcher,
            TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK);
        treeland_keyboard_state_watcher_v1_set_flags(watcher,
            TREELAND_KEYBOARD_STATE_WATCHER_V1_WATCH_FLAG_LOCKED |
            TREELAND_KEYBOARD_STATE_WATCHER_V1_WATCH_FLAG_UNLOCKED);
        treeland_keyboard_state_watcher_v1_apply(watcher);
        wl_display_roundtrip(connection.display);

        if (ws.current_state_count > 0) {

            if (ws.last_modifier !=
                    TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK) {
                fprintf(stderr,
                        "keyboard-state-notify: expected modifier=caps_lock (%u), got %u\n",
                        TREELAND_KEYBOARD_STATE_WATCHER_V1_MODIFIER_CAPS_LOCK,
                        ws.last_modifier);
                treeland_keyboard_state_watcher_v1_destroy(watcher);
                goto failed;
            }

            if (ws.last_state > 1) {
                fprintf(stderr,
                        "keyboard-state-notify: invalid modifier_state %u\n",
                        ws.last_state);
                treeland_keyboard_state_watcher_v1_destroy(watcher);
                goto failed;
            }
        }

        if (ws.state_changed_count != 0) {
            fprintf(stderr,
                    "keyboard-state-notify: unexpected state_changed events: %d\n",
                    ws.state_changed_count);
            treeland_keyboard_state_watcher_v1_destroy(watcher);
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_destroy(watcher);
    }

    {
        struct watcher_state ws = {0};

        struct treeland_keyboard_state_watcher_v1 *watcher =
            treeland_keyboard_state_notify_manager_v1_get_keyboard_state_watcher(manager, seat);
        if (!watcher) {
            fprintf(stderr, "keyboard-state-notify: get_keyboard_state_watcher(seat) failed\n");
            goto failed;
        }

        treeland_keyboard_state_watcher_v1_add_listener(watcher, &watcher_listener, &ws);

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

    treeland_keyboard_state_notify_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    client_disconnect(&connection);
    return 0;

failed:
    client_disconnect(&connection);
    return 1;
}
