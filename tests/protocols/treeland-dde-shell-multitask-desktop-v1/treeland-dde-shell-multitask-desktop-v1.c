// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "treeland-dde-shell-multitask-desktop-v1.h"
#include "treeland-dde-shell-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void dde_multitask_desktop_read_state(void *data);

static int active_state_matches(const struct dde_multitask_desktop_state *state)
{
    return state->output_ready && state->workspace_window_count == 1 && state->multitask_created
        && state->multitask_status == 2 && state->active_reason == 1
        && state->mode_is_multitask && state->partial_factor_milli == 1000;
}

static int exited_state_matches(const struct dde_multitask_desktop_state *state)
{
    return state->multitask_created && state->multitask_status == 3 && state->mode_is_normal
        && state->partial_factor_milli == 0;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct dde_multitask_desktop_state state = { 0 };
    struct treeland_dde_shell_manager_v1 *manager = NULL;
    struct treeland_multitaskview_v1 *multitask = NULL;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        goto done;
    manager = client_bind(&connection,
                                 "treeland_dde_shell_manager_v1",
                                 &treeland_dde_shell_manager_v1_interface,
                                 1);
    if (!manager || !xdg_toplevel_client_create(&connection, &toplevel))
        goto done;
    multitask = treeland_dde_shell_manager_v1_get_treeland_multitaskview(manager);
    if (!multitask)
        goto done;

    treeland_multitaskview_v1_toggle(multitask);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(dde_multitask_desktop_read_state, &state)
        || !active_state_matches(&state))
        goto done;

    treeland_multitaskview_v1_toggle(multitask);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(dde_multitask_desktop_read_state, &state)
        || !exited_state_matches(&state))
        goto done;
    result = 0;
done:
    if (result != 0) {
        fprintf(stderr, "DDE multitask desktop failed: output=%d windows=%d item=%d status=%d reason=%d mode=(multi=%d normal=%d) partial=%d\n", state.output_ready, state.workspace_window_count, state.multitask_created, state.multitask_status, state.active_reason, state.mode_is_multitask, state.mode_is_normal, state.partial_factor_milli);
    }
    xdg_toplevel_client_destroy(&toplevel);
    if (multitask) treeland_multitaskview_v1_destroy(multitask);
    if (manager) treeland_dde_shell_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return result;
}
