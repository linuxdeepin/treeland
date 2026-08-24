// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "viewporter-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    struct wp_viewporter *viewporter =
        client_bind(&conn, "wp_viewporter", &wp_viewporter_interface, 1);
    if (!compositor || !viewporter) {
        fprintf(stderr, "viewporter: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "viewporter: failed to create surface\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Positive: a viewport can be created and crop/scale state applied. */
    struct wp_viewport *viewport = wp_viewporter_get_viewport(viewporter, surface);
    if (!viewport) {
        fprintf(stderr, "viewporter: get_viewport returned null\n");
        goto fail;
    }
    wp_viewport_set_destination(viewport, 64, 64);
    wl_surface_commit(surface);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "viewporter: valid viewport state was rejected\n");
        goto fail;
    }

    /* Negative: a second viewport on the same surface must raise the
     * viewport_exists protocol error. */
    (void)wp_viewporter_get_viewport(viewporter, surface);
    if (wl_display_roundtrip(conn.display) >= 0) {
        fprintf(stderr, "viewporter: duplicate viewport did not raise an error\n");
        goto fail;
    }

    /* Expected error raised; the connection is now fatal and the process
     * will exit without further cleanup. */
    return 0;

fail:
    client_disconnect(&conn);
    return 1;
}
