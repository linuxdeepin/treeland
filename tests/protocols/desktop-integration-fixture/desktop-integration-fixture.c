// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
/*
 * Verifies the production desktop path from an xdg client window to workspace.
 */
#include "desktop-integration-fixture.h"
#include "protocol-test-xdg-client.h"

#include <stdio.h>
#include <string.h>

extern void desktop_fixture_read_state(void *data);

int protocol_test_run(const char *socket_name)
{
    struct protocol_test_connection connection;
    struct protocol_test_xdg_toplevel toplevel;
    struct desktop_fixture_state state;
    if (!protocol_test_connect(&connection, socket_name))
        return 1;
    if (!protocol_test_xdg_toplevel_create(&connection, &toplevel)) {
        protocol_test_disconnect(&connection);
        return 1;
    }
    memset(&state, 0, sizeof(state));
    const int success = protocol_test_invoke_server(desktop_fixture_read_state, &state)
        && state.output_ready && state.wrapper_created && state.wrapper_in_workspace;
    protocol_test_xdg_toplevel_destroy(&toplevel);
    protocol_test_disconnect(&connection);
    if (!success)
        fprintf(stderr, "desktop fixture did not create a workspace SurfaceWrapper\n");
    return success ? 0 : 1;
}
