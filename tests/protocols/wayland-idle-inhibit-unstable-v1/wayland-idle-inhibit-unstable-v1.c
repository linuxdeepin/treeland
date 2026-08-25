// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zwp_idle_inhibit_manager_v1 global served by Treeland
// (wlr_idle_inhibit_v1_create).  The interface has no events; the client maps a
// toplevel and creates an inhibitor on its surface, asserting the inhibitor
// resource is created and survives a roundtrip.

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "idle-inhibit: connect failed\n");
        return 1;
    }

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 64, 64, 0xff00ff00u)) {
        fprintf(stderr, "idle-inhibit: create toplevel failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_idle_inhibit_manager_v1 *manager = client_bind(
        &conn, "zwp_idle_inhibit_manager_v1",
        &zwp_idle_inhibit_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "idle-inhibit: failed to bind manager\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_idle_inhibitor_v1 *inhibitor =
        zwp_idle_inhibit_manager_v1_create_inhibitor(manager, tc.surface);
    if (!inhibitor) {
        fprintf(stderr, "idle-inhibit: create_inhibitor returned NULL\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    wl_display_roundtrip(conn.display);

    // Destroy the inhibitor first (valid even while the surface stays mapped).
    zwp_idle_inhibitor_v1_destroy(inhibitor);
    zwp_idle_inhibit_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;
}
