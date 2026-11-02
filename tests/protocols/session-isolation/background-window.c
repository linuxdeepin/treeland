// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "background-window.h"
#include "session-isolation.h"
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "server-bridge-api.h"

int session_test_background_window(void)
{
    struct isolation_state state = {0};
    struct client_connection client = {0};
    struct xdg_toplevel_client window = {0};
    int result = 1;
    if (!invoke_on_server_thread(isolation_prepare_background, &state) || !state.ready)
        goto cleanup;
    if (!client_connect(&client, state.socket_name) || !xdg_toplevel_client_create(&client, &window))
        goto cleanup;
    xdg_toplevel_set_app_id(window.toplevel, "org.deepin.session-same-app");
    if (wl_display_roundtrip(client.display) < 0
        || !invoke_on_server_thread(isolation_read, &state)
        || state.a_windows != 1 || state.b_windows != 1
        || state.a_visible_windows != 1 || state.b_visible_windows != 0
        || !state.inactive_surfaces_stopped)
        goto cleanup;
    if (!invoke_on_server_thread(isolation_reactivate_b, NULL)
        || wl_display_roundtrip(client.display) < 0
        || !invoke_on_server_thread(isolation_read, &state)
        || state.a_windows != 1 || state.b_windows != 1
        || state.a_visible_windows != 0 || state.b_visible_windows != 1
        || !state.inactive_surfaces_stopped)
        goto cleanup;
    result = 0;
cleanup:
    if (client.display) {
        xdg_toplevel_client_destroy(&window);
        client_disconnect(&client);
    }
    if (!invoke_on_server_thread(isolation_restore_a, NULL)
        || !invoke_on_server_thread(isolation_read, &state)
        || state.a_windows != 1 || state.a_visible_windows != 1)
        result = 1;
    return result;
}
