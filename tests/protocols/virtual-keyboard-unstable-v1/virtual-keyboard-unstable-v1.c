// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

static int expect_no_keymap(const char *socket_name, int modifiers)
{
    struct client_connection connection = {0};
    if (!client_connect(&connection, socket_name))
        return 0;
    struct wl_seat *seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct zwp_virtual_keyboard_manager_v1 *manager = client_bind(
        &connection, "zwp_virtual_keyboard_manager_v1", &zwp_virtual_keyboard_manager_v1_interface, 1);
    if (!seat || !manager)
        goto failed;
    struct zwp_virtual_keyboard_v1 *keyboard =
        zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(manager, seat);
    if (!keyboard)
        goto failed;
    if (modifiers)
        zwp_virtual_keyboard_v1_modifiers(keyboard, 1, 0, 0, 0);
    else
        zwp_virtual_keyboard_v1_key(keyboard, 0, 59, WL_KEYBOARD_KEY_STATE_PRESSED);

    const struct wl_interface *interface = NULL;
    uint32_t code = 0;
    const int rejected = wl_display_roundtrip(connection.display) < 0
        && wl_display_get_protocol_error(connection.display, &interface, &code)
            == ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP
        && interface == &zwp_virtual_keyboard_v1_interface;
    client_disconnect(&connection);
    return rejected;

failed:
    client_disconnect(&connection);
    return 0;
}

int protocol_test_run(const char *socket_name)
{
    return expect_no_keymap(socket_name, 0) && expect_no_keymap(socket_name, 1) ? 0 : 1;
}
