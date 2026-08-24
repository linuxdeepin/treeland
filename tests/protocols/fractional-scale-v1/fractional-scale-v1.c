// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "fractional-scale-v1-client-protocol.h"

#include <stdio.h>

struct scale_info {
    int received;
    uint32_t scale;
};

static void handle_preferred_scale(void *data,
                                   struct wp_fractional_scale_v1 *fs,
                                   uint32_t scale)
{
    (void)fs;
    struct scale_info *info = data;
    info->received = 1;
    info->scale = scale;
}

static const struct wp_fractional_scale_v1_listener listener = {
    .preferred_scale = handle_preferred_scale,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "fractional-scale: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_fractional_scale_manager_v1 *manager =
        client_bind(&conn, "wp_fractional_scale_manager_v1",
                    &wp_fractional_scale_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "fractional-scale: failed to bind manager\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct wp_fractional_scale_v1 *fs =
        wp_fractional_scale_manager_v1_get_fractional_scale(manager, tc.surface);

    struct scale_info info = {0};
    wp_fractional_scale_v1_add_listener(fs, &listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "fractional-scale: roundtrip failed\n");
        wp_fractional_scale_v1_destroy(fs);
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    /* The headless output has scale 1, so preferred_scale should be 120. */
    if (!info.received || info.scale == 0) {
        fprintf(stderr, "fractional-scale: did not receive preferred_scale event\n");
        wp_fractional_scale_v1_destroy(fs);
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    wp_fractional_scale_v1_destroy(fs);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;
}
