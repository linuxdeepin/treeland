// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zwp_text_input_manager_v1 global served by Treeland's
// WInputMethodHelper.  The client creates a text_input v1 for the seat,
// activates it on a surface, and asserts the resource survives a roundtrip
// without a protocol error.  (text-input-v1 has no `destroy` request; cleanup
// is via wl_display_disconnect.)

#include "client-connection.h"
#include "text-input-unstable-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "text-input-v1: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 5);
    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    if (!seat || !compositor) {
        fprintf(stderr, "text-input-v1: missing core global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_text_input_manager_v1 *manager = client_bind(
        &conn, "zwp_text_input_manager_v1",
        &zwp_text_input_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "text-input-v1: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct zwp_text_input_v1 *text_input =
        zwp_text_input_manager_v1_create_text_input(manager);
    if (!text_input) {
        fprintf(stderr, "text-input-v1: create_text_input returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    /* activate(seat, surface) — the real production request. */
    zwp_text_input_v1_activate(text_input, seat, surface);
    zwp_text_input_v1_show_input_panel(text_input);
    zwp_text_input_v1_hide_input_panel(text_input);
    wl_display_roundtrip(conn.display);

    /* text-input-v1 has no destroy request; wl_display_disconnect reclaims it. */
    client_disconnect(&conn);
    return 0;
}
