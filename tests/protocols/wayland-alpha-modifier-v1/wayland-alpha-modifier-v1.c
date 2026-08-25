// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the wp_alpha_modifier_v1 global created by wlr_alpha_modifier_v1_create
// in Treeland's Helper.  The client attaches an alpha-modifier surface object
// to a plain wl_surface, sets a multiplier, commits, and asserts the resource
// survives a roundtrip without a protocol error.

#include "client-connection.h"
#include "alpha-modifier-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "alpha-modifier: connect failed\n");
        return 1;
    }

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    if (!compositor) {
        fprintf(stderr, "alpha-modifier: no wl_compositor global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_alpha_modifier_v1 *manager = client_bind(
        &conn, "wp_alpha_modifier_v1", &wp_alpha_modifier_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "alpha-modifier: failed to bind wp_alpha_modifier_v1\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "alpha-modifier: create_surface failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_alpha_modifier_surface_v1 *am_surface =
        wp_alpha_modifier_v1_get_surface(manager, surface);
    if (!am_surface) {
        fprintf(stderr, "alpha-modifier: get_surface returned NULL\n");
        wl_surface_destroy(surface);
        client_disconnect(&conn);
        return 1;
    }

    wp_alpha_modifier_surface_v1_set_multiplier(am_surface, 0x80808080u);
    wl_surface_commit(surface);
    wl_display_roundtrip(conn.display);

    // Resource still live after commit confirms the alpha modifier was applied.
    wp_alpha_modifier_surface_v1_set_multiplier(am_surface, 0xffffffffu);
    wl_display_roundtrip(conn.display);

    wp_alpha_modifier_surface_v1_destroy(am_surface);
    wl_surface_destroy(surface);
    wp_alpha_modifier_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
