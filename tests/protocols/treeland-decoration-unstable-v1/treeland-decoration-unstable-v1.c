// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-decoration-unstable-v1.h"
#include "xdg-toplevel-client.h"
#include "treeland-decoration-unstable-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    if (!client_connect(&connection, socket_name)
        || !xdg_toplevel_client_create(&connection, &toplevel))
        return 1;
    struct treeland_decoration_manager_v1 *manager = client_bind(&connection,
        "treeland_decoration_manager_v1", &treeland_decoration_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "decoration: manager missing\n");
        return 1;
    }
    struct treeland_decoration_context_v1 *context =
        treeland_decoration_manager_v1_get_decoration_context(manager, toplevel.surface);
    if (!context) {
        fprintf(stderr, "decoration: context creation failed\n");
        return 1;
    }
    treeland_decoration_context_v1_set_corner_radius(context, 12);
    if (wl_display_roundtrip(connection.display) < 0)
        return 1;
    struct decoration_server_state state = { 0 };
    if (!invoke_on_server_thread(decoration_read_server_state, &state)
        || !state.valid || state.radius != 12.0) {
        fprintf(stderr, "decoration: corner radius was not applied (valid=%d radius=%g)\n",
                state.valid, state.radius);
        return 1;
    }
    treeland_decoration_context_v1_destroy(context);
    treeland_decoration_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;
}
