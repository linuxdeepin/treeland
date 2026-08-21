// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-shortcut-manager-desktop-v2.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "treeland-shortcut-manager-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

extern void shortcut_desktop_focus_window(void *data);

struct shortcut_client {
    int commit_success;
    int commit_failure;
    int activated;
    char activated_name[64];
    unsigned int activated_flags;
    int captured;
    char captured_key[64];
    int capture_failed;
};

static void manager_activated(void *data,
                              struct treeland_shortcut_manager_v2 *manager,
                              const char *name,
                              uint32_t flags)
{
    (void)manager;
    struct shortcut_client *client = data;
    ++client->activated;
    client->activated_flags = flags;
    strncpy(client->activated_name, name, sizeof(client->activated_name) - 1);
}

static void manager_commit_success(void *data, struct treeland_shortcut_manager_v2 *manager)
{
    (void)manager;
    ++((struct shortcut_client *)data)->commit_success;
}

static void manager_commit_failure(void *data,
                                   struct treeland_shortcut_manager_v2 *manager,
                                   const char *name,
                                   uint32_t error)
{
    (void)manager;
    (void)name;
    (void)error;
    ++((struct shortcut_client *)data)->commit_failure;
}

static const struct treeland_shortcut_manager_v2_listener manager_listener = {
    .activated = manager_activated,
    .commit_success = manager_commit_success,
    .commit_failure = manager_commit_failure,
};

static void capture_captured(void *data, struct treeland_shortcut_capture_v1 *capture, const char *key)
{
    (void)capture;
    struct shortcut_client *client = data;
    ++client->captured;
    strncpy(client->captured_key, key, sizeof(client->captured_key) - 1);
}

static void capture_failed(void *data, struct treeland_shortcut_capture_v1 *capture, uint32_t reason)
{
    (void)capture;
    (void)reason;
    ++((struct shortcut_client *)data)->capture_failed;
}

static const struct treeland_shortcut_capture_v1_listener capture_listener = {
    .captured = capture_captured,
    .failed = capture_failed,
};

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
    snprintf(name, sizeof(name), "/treeland_shortcut_keymap_%d", (int)getpid());
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    shm_unlink(name);
    if (fd < 0)
        goto failed;
    const size_t size = strlen(keymap_text) + 1;
    if (ftruncate(fd, (off_t)size) < 0 || write(fd, keymap_text, size) != (ssize_t)size) {
        close(fd);
        goto failed;
    }
    zwp_virtual_keyboard_v1_keymap(keyboard,
                                   WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                   fd,
                                   (uint32_t)size);
    close(fd);
    free(keymap_text);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    return 1;

failed:
    if (keymap_text) free(keymap_text);
    if (keymap) xkb_keymap_unref(keymap);
    if (context) xkb_context_unref(context);
    return 0;
}

static void send_key(struct zwp_virtual_keyboard_v1 *keyboard, uint32_t key, uint32_t state)
{
    zwp_virtual_keyboard_v1_key(keyboard, 0, key, state);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct treeland_shortcut_manager_v2 *manager = NULL;
    struct treeland_shortcut_capture_v1 *capture = NULL;
    struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager = NULL;
    struct zwp_virtual_keyboard_v1 *virtual_keyboard = NULL;
    struct wl_seat *seat = NULL;
    struct shortcut_desktop_state state = { 0 };
    struct shortcut_client client = { 0 };
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;
    manager = client_bind(&connection, "treeland_shortcut_manager_v2",
                                 &treeland_shortcut_manager_v2_interface, 2);
    seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    virtual_keyboard_manager = client_bind(&connection, "zwp_virtual_keyboard_manager_v1",
                                                   &zwp_virtual_keyboard_manager_v1_interface, 1);
    if (!manager || !seat || !virtual_keyboard_manager)
        goto failed;
    treeland_shortcut_manager_v2_add_listener(manager, &manager_listener, &client);
    stage = 1;
    if (!xdg_toplevel_client_create(&connection, &toplevel))
        goto failed;
    stage = 2;
    if (!invoke_on_server_thread(shortcut_desktop_focus_window, &state)
        || !state.wrapper_created || !state.wrapper_in_workspace || !state.wrapper_visible
        || !state.keyboard_focused)
        goto failed;

    virtual_keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
        virtual_keyboard_manager, seat);
    if (!virtual_keyboard || !send_keymap(virtual_keyboard)
        || wl_display_roundtrip(connection.display) < 0)
        goto failed;

    treeland_shortcut_manager_v2_acquire(manager);
    capture = treeland_shortcut_manager_v2_capture_next_shortcut(manager, toplevel.surface, seat);
    if (!capture)
        goto failed;
    treeland_shortcut_capture_v1_add_listener(capture, &capture_listener, &client);
    if (wl_display_roundtrip(connection.display) < 0 || client.capture_failed)
        goto failed;
    stage = 3;
    send_key(virtual_keyboard, 59, WL_KEYBOARD_KEY_STATE_PRESSED); // KEY_F1
    if (wl_display_roundtrip(connection.display) < 0 || client.captured != 1
        || strcmp(client.captured_key, "F1") != 0)
        goto failed;
    stage = 4;
    send_key(virtual_keyboard, 59, WL_KEYBOARD_KEY_STATE_RELEASED); // KEY_F1
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;

    treeland_shortcut_manager_v2_bind_key(
        manager, "desktop-shortcut", "F2",
        TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_FLAG_KEY_PRESS,
        TREELAND_SHORTCUT_MANAGER_V2_ACTION_NOTIFY);
    treeland_shortcut_manager_v2_commit(manager);
    if (wl_display_roundtrip(connection.display) < 0 || client.commit_success != 1
        || client.commit_failure)
        goto failed;
    stage = 5;
    send_key(virtual_keyboard, 60, WL_KEYBOARD_KEY_STATE_PRESSED); // KEY_F2
    if (wl_display_roundtrip(connection.display) < 0 || client.activated != 1
        || strcmp(client.activated_name, "desktop-shortcut") != 0
        || client.activated_flags != TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_FLAG_KEY_PRESS)
        goto failed;
    stage = 6;

    treeland_shortcut_capture_v1_destroy(capture);
    zwp_virtual_keyboard_v1_destroy(virtual_keyboard);
    treeland_shortcut_manager_v2_destroy(manager);
    wl_seat_destroy(seat);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "shortcut desktop failure at stage %d: wrapper=%d workspace=%d visible=%d focus=%d "
            "captured=%d key=%s failed=%d commit=(%d,%d) activated=%d name=%s flags=%u\n",
            stage, state.wrapper_created, state.wrapper_in_workspace, state.wrapper_visible,
            state.keyboard_focused, client.captured, client.captured_key, client.capture_failed,
            client.commit_success, client.commit_failure, client.activated, client.activated_name,
            client.activated_flags);
    if (capture) treeland_shortcut_capture_v1_destroy(capture);
    if (virtual_keyboard) zwp_virtual_keyboard_v1_destroy(virtual_keyboard);
    if (manager) treeland_shortcut_manager_v2_destroy(manager);
    if (seat) wl_seat_destroy(seat);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 1;
}
