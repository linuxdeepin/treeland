// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-dialog-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "xdg-dialog: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct xdg_wm_dialog_v1 *manager =
        client_bind(&conn, "xdg_wm_dialog_v1", &xdg_wm_dialog_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "xdg-dialog: failed to bind xdg_wm_dialog_v1\n");
        goto fail;
    }

    /* Positive: register the toplevel as a modal dialog. */
    struct xdg_dialog_v1 *dialog = xdg_wm_dialog_v1_get_xdg_dialog(manager, tc.toplevel);
    if (!dialog) {
        fprintf(stderr, "xdg-dialog: get_xdg_dialog returned null\n");
        goto fail;
    }
    xdg_dialog_v1_set_modal(dialog);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-dialog: dialog creation was rejected\n");
        goto fail;
    }

    /* Negative: a second dialog for the same toplevel must raise already_used. */
    (void)xdg_wm_dialog_v1_get_xdg_dialog(manager, tc.toplevel);
    if (wl_display_roundtrip(conn.display) >= 0) {
        fprintf(stderr, "xdg-dialog: duplicate dialog did not raise an error\n");
        goto fail;
    }

    return 0;

fail:
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
