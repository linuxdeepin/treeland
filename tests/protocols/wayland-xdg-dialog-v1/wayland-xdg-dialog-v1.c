// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the xdg_wm_dialog_v1 global served by Treeland
// (wlr_xdg_dialog_v1_create).  The interface has no events; the client attaches
// a dialog object to a toplevel, marks it modal, maps it, and asserts the
// resource survives without a protocol error — exercising the real dialog path.

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-dialog-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "xdg-dialog: connect failed\n");
        return 1;
    }

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
        fprintf(stderr, "xdg-dialog: create_pending failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct xdg_wm_dialog_v1 *manager = client_bind(
        &conn, "xdg_wm_dialog_v1", &xdg_wm_dialog_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "xdg-dialog: failed to bind xdg_wm_dialog_v1\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct xdg_dialog_v1 *dialog =
        xdg_wm_dialog_v1_get_xdg_dialog(manager, tc.toplevel);
    if (!dialog) {
        fprintf(stderr, "xdg-dialog: get_xdg_dialog returned NULL\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }
    xdg_dialog_v1_set_modal(dialog);

    if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
        fprintf(stderr, "xdg-dialog: complete_map failed\n");
        xdg_dialog_v1_destroy(dialog);
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }
    wl_display_roundtrip(conn.display);

    xdg_dialog_v1_destroy(dialog);
    xdg_wm_dialog_v1_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;
}
