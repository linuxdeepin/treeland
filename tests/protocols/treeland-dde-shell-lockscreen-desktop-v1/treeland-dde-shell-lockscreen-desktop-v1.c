// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-dde-shell-lockscreen-desktop-v1.h"
#include "treeland-dde-shell-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void dde_lockscreen_desktop_read_state(void *data);

static int initial_state_matches(const struct dde_lockscreen_desktop_state *state)
{
    return state->output_ready && state->lockscreen_available && !state->lockscreen_locked
        && state->mode_is_normal;
}

static int locked_state_matches(const struct dde_lockscreen_desktop_state *state)
{
    return state->output_ready && state->lockscreen_available && state->lockscreen_locked
        && state->mode_is_lockscreen;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct dde_lockscreen_desktop_state state = { 0 };
    struct treeland_dde_shell_manager_v1 *manager = NULL;
    struct treeland_lockscreen_v1 *lockscreen = NULL;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        goto done;
    manager = client_bind(&connection,
                                 "treeland_dde_shell_manager_v1",
                                 &treeland_dde_shell_manager_v1_interface,
                                 1);
    if (!manager
        || !invoke_on_server_thread(dde_lockscreen_desktop_read_state, &state)
        || !initial_state_matches(&state))
        goto done;

    lockscreen = treeland_dde_shell_manager_v1_get_treeland_lockscreen(manager);
    if (!lockscreen)
        goto done;
    treeland_lockscreen_v1_lock(lockscreen);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(dde_lockscreen_desktop_read_state, &state)
        || !locked_state_matches(&state))
        goto done;

    result = 0;
done:
    if (result != 0) {
        fprintf(stderr,
                "DDE lockscreen desktop failed: output=%d available=%d locked=%d "
                "normal=%d lock-mode=%d\n",
                state.output_ready,
                state.lockscreen_available,
                state.lockscreen_locked,
                state.mode_is_normal,
                state.mode_is_lockscreen);
    }
    if (lockscreen)
        treeland_lockscreen_v1_destroy(lockscreen);
    if (manager)
        treeland_dde_shell_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return result;
}
