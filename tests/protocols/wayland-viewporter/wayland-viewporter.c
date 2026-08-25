// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the wp_viewporter global created by wlr_viewporter_create in Treeland's
// Helper.  The client creates a viewport for a plain wl_surface, applies a
// destination size, commits, and asserts the viewport resource survives a
// roundtrip without a protocol error — exercising the real wlroots viewporter.

#include "client-connection.h"
#include "viewporter-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "viewporter: connect failed\n");
        return 1;
    }

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    if (!compositor) {
        fprintf(stderr, "viewporter: no wl_compositor global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_viewporter *viewporter = client_bind(
        &conn, "wp_viewporter", &wp_viewporter_interface, 1);
    if (!viewporter) {
        fprintf(stderr, "viewporter: failed to bind wp_viewporter\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "viewporter: create_surface failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_viewport *viewport = wp_viewporter_get_viewport(viewporter, surface);
    if (!viewport) {
        fprintf(stderr, "viewporter: get_viewport returned NULL\n");
        wl_surface_destroy(surface);
        client_disconnect(&conn);
        return 1;
    }

    wp_viewport_set_destination(viewport, 320, 240);
    wl_surface_commit(surface);
    wl_display_roundtrip(conn.display);

    // No protocol error means the viewporter accepted the viewport + commit.
    // Re-issue a harmless operation to confirm the resource is still live.
    wp_viewport_set_source(viewport, wl_fixed_from_int(0), wl_fixed_from_int(0),
                           wl_fixed_from_int(320), wl_fixed_from_int(240));
    wl_display_roundtrip(conn.display);

    wp_viewport_destroy(viewport);
    wl_surface_destroy(surface);
    wp_viewporter_destroy(viewporter);
    client_disconnect(&conn);
    return 0;
}
