// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "treeland-dde-shell-picker-desktop-v1.h"
#include "treeland-dde-shell-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern void dde_picker_desktop_select_mapped_window(void *data);

struct picker_client_state {
    int received;
    int pid;
};

static void picker_window(void *data, struct treeland_window_picker_v1 *picker, int32_t pid)
{
    (void)picker;
    struct picker_client_state *state = data;
    state->received = 1;
    state->pid = pid;
}

static const struct treeland_window_picker_v1_listener picker_listener = {
    .window = picker_window,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct dde_picker_desktop_state state = { 0 };
    struct picker_client_state picker_state = { 0 };
    struct treeland_dde_shell_manager_v1 *manager = NULL;
    struct treeland_window_picker_v1 *picker = NULL;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        goto done;
    manager = client_bind(&connection,
                                 "treeland_dde_shell_manager_v1",
                                 &treeland_dde_shell_manager_v1_interface,
                                 1);
    if (!manager || !xdg_toplevel_client_create(&connection, &toplevel))
        goto done;
    picker = treeland_dde_shell_manager_v1_get_treeland_window_picker(manager);
    if (!picker || treeland_window_picker_v1_add_listener(picker, &picker_listener, &picker_state))
        goto done;
    treeland_window_picker_v1_pick(picker, "protocol picker");
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(dde_picker_desktop_select_mapped_window, &state)
        || wl_display_roundtrip(connection.display) < 0
        || !state.output_ready || !state.wrapper_ready || !state.wrapper_in_workspace
        || !state.picker_created || !state.mapped_window_selected
        || !picker_state.received || picker_state.pid != (int)getpid())
        goto done;
    result = 0;
done:
    if (result != 0) {
        fprintf(stderr, "DDE picker desktop failed: wrapper=(ready=%d workspace=%d) manager=(found=%d resource=%d request=%d) picker=(objects=%d instances=%d created=%d selected=%d) event=(received=%d pid=%d expected=%d)\n", state.wrapper_ready, state.wrapper_in_workspace, state.manager_found, state.picker_resource_created, state.pick_request_received, state.root_object_count, state.window_picker_instances, state.picker_created, state.mapped_window_selected, picker_state.received, picker_state.pid, (int)getpid());
    }
    xdg_toplevel_client_destroy(&toplevel);
    if (picker) treeland_window_picker_v1_destroy(picker);
    if (manager) treeland_dde_shell_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return result;
}
