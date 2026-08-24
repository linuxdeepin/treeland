// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "text-input-unstable-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "text-input-v1: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    if (!seat) {
        fprintf(stderr, "text-input-v1: failed to bind seat\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_text_input_manager_v1 *manager =
        client_bind(&conn, "zwp_text_input_manager_v1",
                    &zwp_text_input_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "text-input-v1: failed to bind manager\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_text_input_v1 *ti = zwp_text_input_manager_v1_create_text_input(manager);

    /* Lifecycle: activate → commit_state → deactivate.
     * Without keyboard focus no enter event is expected; the assertion
     * is that each step completes without a protocol error. */
    zwp_text_input_v1_activate(ti, seat, tc.surface);
    zwp_text_input_v1_commit_state(ti, 0);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "text-input-v1: roundtrip after activate failed\n");
        zwp_text_input_v1_destroy(ti);
        zwp_text_input_manager_v1_destroy(manager);
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    zwp_text_input_v1_deactivate(ti, seat);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "text-input-v1: roundtrip after deactivate failed\n");
        zwp_text_input_v1_destroy(ti);
        zwp_text_input_manager_v1_destroy(manager);
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    zwp_text_input_v1_destroy(ti);
    zwp_text_input_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;
}
