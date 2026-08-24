// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-toplevel-tag-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "xdg-toplevel-tag: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct xdg_toplevel_tag_manager_v1 *manager =
        client_bind(&conn, "xdg_toplevel_tag_manager_v1",
                    &xdg_toplevel_tag_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "xdg-toplevel-tag: failed to bind manager\n");
        goto fail;
    }

    /* Positive: set tag and description on a mapped toplevel. */
    xdg_toplevel_tag_manager_v1_set_toplevel_tag(manager, tc.toplevel, "test-tag");
    xdg_toplevel_tag_manager_v1_set_toplevel_description(manager, tc.toplevel,
                                                          "test-description");
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-toplevel-tag: requests were rejected\n");
        goto fail;
    }

    xdg_toplevel_tag_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;

fail:
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
