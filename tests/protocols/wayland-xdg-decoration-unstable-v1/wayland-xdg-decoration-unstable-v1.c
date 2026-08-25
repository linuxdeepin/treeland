// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zxdg_decoration_manager_v1 global served by Treeland
// (wlr_xdg_decoration_manager_v1_create).  The client attaches a decoration
// object to a toplevel, requests client-side mode, maps the toplevel, and
// asserts the server delivers a `configure` event carrying a decoration mode.

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct deco_state {
    int configure_count;
    uint32_t mode;
};

static void deco_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco,
                           uint32_t mode)
{
    (void)deco;
    struct deco_state *state = data;
    state->configure_count++;
    state->mode = mode;
}

static const struct zxdg_toplevel_decoration_v1_listener deco_listener = {
    .configure = deco_configure,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "xdg-decoration: connect failed\n");
        return 1;
    }

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
        fprintf(stderr, "xdg-decoration: create_pending failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zxdg_decoration_manager_v1 *manager = client_bind(
        &conn, "zxdg_decoration_manager_v1",
        &zxdg_decoration_manager_v1_interface, 2);
    if (!manager) {
        fprintf(stderr, "xdg-decoration: failed to bind manager\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct deco_state state;
    memset(&state, 0, sizeof(state));
    struct zxdg_toplevel_decoration_v1 *deco =
        zxdg_decoration_manager_v1_get_toplevel_decoration(manager, tc.toplevel);
    if (!deco) {
        fprintf(stderr, "xdg-decoration: get_toplevel_decoration returned NULL\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }
    zxdg_toplevel_decoration_v1_add_listener(deco, &deco_listener, &state);
    zxdg_toplevel_decoration_v1_set_mode(deco, 1 /* CLIENT_SIDE */);

    if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
        fprintf(stderr, "xdg-decoration: complete_map failed\n");
        zxdg_toplevel_decoration_v1_destroy(deco);
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }
    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (state.configure_count < 1) {
        fprintf(stderr, "xdg-decoration: no configure event received\n");
        failed = 1;
    }

    zxdg_toplevel_decoration_v1_destroy(deco);
    zxdg_decoration_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return failed;
}
