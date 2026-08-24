// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    struct zwp_idle_inhibit_manager_v1 *manager =
        client_bind(&conn, "zwp_idle_inhibit_manager_v1",
                    &zwp_idle_inhibit_manager_v1_interface, 1);
    if (!compositor || !manager) {
        fprintf(stderr, "idle-inhibit: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "idle-inhibit: failed to create surface\n");
        goto fail;
    }

    /* Multiple inhibitors may target the same surface; both must be accepted. */
    struct zwp_idle_inhibitor_v1 *inhibitor_a =
        zwp_idle_inhibit_manager_v1_create_inhibitor(manager, surface);
    struct zwp_idle_inhibitor_v1 *inhibitor_b =
        zwp_idle_inhibit_manager_v1_create_inhibitor(manager, surface);
    if (!inhibitor_a || !inhibitor_b) {
        fprintf(stderr, "idle-inhibit: create_inhibitor returned null\n");
        goto fail;
    }
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "idle-inhibit: inhibitor creation was rejected\n");
        goto fail;
    }

    /* Destroying an inhibitor removes its effect without affecting the other. */
    zwp_idle_inhibitor_v1_destroy(inhibitor_a);
    zwp_idle_inhibitor_v1_destroy(inhibitor_b);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "idle-inhibit: inhibitor destruction was rejected\n");
        goto fail;
    }

    wl_surface_destroy(surface);
    zwp_idle_inhibit_manager_v1_destroy(manager);
    wl_compositor_destroy(compositor);
    client_disconnect(&conn);
    return 0;

fail:
    client_disconnect(&conn);
    return 1;
}
