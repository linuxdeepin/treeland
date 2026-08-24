// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "alpha-modifier-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    struct wp_alpha_modifier_v1 *manager =
        client_bind(&conn, "wp_alpha_modifier_v1", &wp_alpha_modifier_v1_interface, 1);
    if (!compositor || !manager) {
        fprintf(stderr, "alpha-modifier: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "alpha-modifier: failed to create surface\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Positive: create an alpha modifier surface and set a multiplier. */
    struct wp_alpha_modifier_surface_v1 *mod =
        wp_alpha_modifier_v1_get_surface(manager, surface);
    if (!mod) {
        fprintf(stderr, "alpha-modifier: get_surface returned null\n");
        goto fail;
    }
    wp_alpha_modifier_surface_v1_set_multiplier(mod, 0x80808080u);
    wl_surface_commit(surface);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "alpha-modifier: valid multiplier was rejected\n");
        goto fail;
    }

    /* Negative: a second modifier on the same surface must raise
     * already_constructed. */
    (void)wp_alpha_modifier_v1_get_surface(manager, surface);
    if (wl_display_roundtrip(conn.display) >= 0) {
        fprintf(stderr, "alpha-modifier: duplicate modifier did not raise an error\n");
        goto fail;
    }

    return 0;

fail:
    client_disconnect(&conn);
    return 1;
}
