// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the xdg_toplevel_tag_manager_v1 global served by Treeland
// (wlr_xdg_toplevel_tag_manager_v1_create).  The interface has only requests
// (set_toplevel_tag / set_toplevel_description) and no events; the client
// applies a tag + description to a real toplevel and maps it without error.

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-toplevel-tag-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "xdg-toplevel-tag: connect failed\n");
        return 1;
    }

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
        fprintf(stderr, "xdg-toplevel-tag: create_pending failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct xdg_toplevel_tag_manager_v1 *manager = client_bind(
        &conn, "xdg_toplevel_tag_manager_v1",
        &xdg_toplevel_tag_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "xdg-toplevel-tag: failed to bind manager\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    xdg_toplevel_tag_manager_v1_set_toplevel_tag(manager, tc.toplevel, "treeland-tag-test");
    xdg_toplevel_tag_manager_v1_set_toplevel_description(manager, tc.toplevel,
                                                         "Protocol Test Window");

    if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
        fprintf(stderr, "xdg-toplevel-tag: complete_map failed\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }
    wl_display_roundtrip(conn.display);

    xdg_toplevel_tag_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;
}
