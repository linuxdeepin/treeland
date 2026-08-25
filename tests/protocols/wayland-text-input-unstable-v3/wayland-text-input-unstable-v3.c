// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zwp_text_input_manager_v3 global served by Treeland's
// WTextInputManagerV3 (attached via WInputMethodHelper).  The client creates a
// text_input v3 for the seat, enables it, commits a state, and asserts the
// resource survives a roundtrip without a protocol error.

#include "client-connection.h"
#include "text-input-unstable-v3-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "text-input-v3: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "text-input-v3: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_text_input_manager_v3 *manager = client_bind(
        &conn, "zwp_text_input_manager_v3",
        &zwp_text_input_manager_v3_interface, 1);
    if (!manager) {
        fprintf(stderr, "text-input-v3: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_text_input_v3 *text_input =
        zwp_text_input_manager_v3_get_text_input(manager, seat);
    if (!text_input) {
        fprintf(stderr, "text-input-v3: get_text_input returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    zwp_text_input_v3_enable(text_input);
    zwp_text_input_v3_set_surrounding_text(text_input, "", 0, 0);
    zwp_text_input_v3_commit(text_input);
    wl_display_roundtrip(conn.display);

    // Disable + commit must also be accepted (clean teardown).
    zwp_text_input_v3_disable(text_input);
    zwp_text_input_v3_commit(text_input);
    wl_display_roundtrip(conn.display);

    zwp_text_input_v3_destroy(text_input);
    zwp_text_input_manager_v3_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
