// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"

#include <stdio.h>

extern void keyboard_shortcuts_inhibit_focus(void *);

struct events { unsigned active; unsigned inactive; };
static void active(void *data, struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{ (void)inhibitor; ((struct events *)data)->active++; }
static void inactive(void *data, struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{ (void)inhibitor; ((struct events *)data)->inactive++; }
static const struct zwp_keyboard_shortcuts_inhibitor_v1_listener listener = { active, inactive };

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct events events = { 0 };
    if (!client_connect(&connection, socket_name)
        || !xdg_toplevel_client_create(&connection, &toplevel)
        || !invoke_on_server_thread(keyboard_shortcuts_inhibit_focus, NULL))
        return 1;
    struct wl_seat *seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct zwp_keyboard_shortcuts_inhibit_manager_v1 *manager = client_bind(&connection,
        "zwp_keyboard_shortcuts_inhibit_manager_v1",
        &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1);
    if (!seat || !manager)
        return 1;
    struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor =
        zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(manager, toplevel.surface, seat);
    if (!inhibitor)
        return 1;
    zwp_keyboard_shortcuts_inhibitor_v1_add_listener(inhibitor, &listener, &events);
    if (wl_display_roundtrip(connection.display) < 0 || events.active != 1) {
        fprintf(stderr, "keyboard-shortcuts-inhibit: active event missing\n");
        return 1;
    }
    zwp_keyboard_shortcuts_inhibitor_v1_destroy(inhibitor);
    zwp_keyboard_shortcuts_inhibit_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;
}
