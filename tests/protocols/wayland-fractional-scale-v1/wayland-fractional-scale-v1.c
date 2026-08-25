// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the wp_fractional_scale_manager_v1 global served by Treeland
// (wlr_fractional_scale_manager_v1_create).  The client maps a toplevel with a
// solid buffer, attaches a fractional-scale object, commits, and asserts the
// server delivers a `preferred_scale` event.

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "fractional-scale-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct fs_state {
    int preferred_scale_count;
    uint32_t scale;
};

static void preferred_scale(void *data, struct wp_fractional_scale_v1 *fs,
                            uint32_t scale)
{
    (void)fs;
    struct fs_state *state = data;
    state->preferred_scale_count++;
    state->scale = scale;
}

static const struct wp_fractional_scale_v1_listener fs_listener = {
    .preferred_scale = preferred_scale,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "fractional-scale: connect failed\n");
        return 1;
    }

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 64, 64, 0xffff0000u)) {
        fprintf(stderr, "fractional-scale: create toplevel failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_fractional_scale_manager_v1 *manager = client_bind(
        &conn, "wp_fractional_scale_manager_v1",
        &wp_fractional_scale_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "fractional-scale: failed to bind manager\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct fs_state state;
    memset(&state, 0, sizeof(state));
    struct wp_fractional_scale_v1 *fs =
        wp_fractional_scale_manager_v1_get_fractional_scale(manager, tc.surface);
    if (!fs) {
        fprintf(stderr, "fractional-scale: get_fractional_scale returned NULL\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }
    wp_fractional_scale_v1_add_listener(fs, &fs_listener, &state);

    wl_surface_commit(tc.surface);
    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (state.preferred_scale_count < 1) {
        fprintf(stderr, "fractional-scale: no preferred_scale event received\n");
        failed = 1;
    } else if (state.scale == 0) {
        fprintf(stderr, "fractional-scale: preferred_scale reported 0\n");
        failed = 1;
    }

    wp_fractional_scale_v1_destroy(fs);
    wp_fractional_scale_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return failed;
}
