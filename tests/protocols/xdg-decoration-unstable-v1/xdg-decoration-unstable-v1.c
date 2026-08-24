// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <stdio.h>

struct deco_info {
    int configured;
    uint32_t mode;
};

static void handle_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco,
                             uint32_t mode)
{
    (void)deco;
    struct deco_info *info = data;
    info->configured = 1;
    info->mode = mode;
}

static const struct zxdg_toplevel_decoration_v1_listener listener = {
    .configure = handle_configure,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
        fprintf(stderr, "xdg-decoration: failed to create pending toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zxdg_decoration_manager_v1 *manager =
        client_bind(&conn, "zxdg_decoration_manager_v1",
                    &zxdg_decoration_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "xdg-decoration: failed to bind manager\n");
        goto fail;
    }

    /* Create the decoration BEFORE mapping (before a buffer is committed).
     * The spec forbids creating a decoration on a surface that already has a
     * committed buffer. */
    struct zxdg_toplevel_decoration_v1 *deco =
        zxdg_decoration_manager_v1_get_toplevel_decoration(manager, tc.toplevel);
    if (!deco) {
        fprintf(stderr, "xdg-decoration: get_toplevel_decoration returned null\n");
        goto fail;
    }

    struct deco_info info = {0};
    zxdg_toplevel_decoration_v1_add_listener(deco, &listener, &info);

    /* Complete the map: ack the initial configure and attach a buffer. */
    if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
        fprintf(stderr, "xdg-decoration: failed to complete map\n");
        goto fail;
    }
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-decoration: roundtrip after get_toplevel_decoration failed\n");
        goto fail;
    }

    if (!info.configured) {
        fprintf(stderr, "xdg-decoration: did not receive configure event\n");
        goto fail;
    }
    /* Mode must be client_side(1) or server_side(2). */
    if (info.mode != 1 && info.mode != 2) {
        fprintf(stderr, "xdg-decoration: invalid mode %u\n", info.mode);
        goto fail;
    }

    /* Negative: a second decoration for the same toplevel must raise
     * already_constructed. */
    (void)zxdg_decoration_manager_v1_get_toplevel_decoration(manager, tc.toplevel);
    if (wl_display_roundtrip(conn.display) >= 0) {
        fprintf(stderr, "xdg-decoration: duplicate did not raise an error\n");
        goto fail;
    }

    return 0;

fail:
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
