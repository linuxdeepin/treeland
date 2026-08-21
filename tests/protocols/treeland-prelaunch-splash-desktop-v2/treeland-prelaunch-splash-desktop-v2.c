// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-prelaunch-splash-desktop-v2.h"
#include "treeland-prelaunch-splash-v2-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void prelaunch_splash_desktop_read_state(void *data);
extern void prelaunch_splash_desktop_wait_for_creation(void *data);
extern void prelaunch_splash_desktop_wait_for_destruction(void *data);

static const char *const splash_app_id = "org.deepin.treeland.protocol.splash";

static int state_has_production_splash(const struct prelaunch_splash_desktop_state *state)
{
    return state->output_ready && state->wrapper_created && state->wrapper_in_workspace
        && state->wrapper_is_splash && state->wrapper_has_qml_item
        && state->wrapper_width == 800 && state->wrapper_height == 600
        && strcmp(state->app_id, splash_app_id) == 0;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct prelaunch_splash_desktop_state state = { 0 };
    struct treeland_prelaunch_splash_manager_v2 *manager = NULL;
    struct treeland_prelaunch_splash_v2 *splash = NULL;
    int creation_observed = 0;
    int destruction_observed = 0;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        goto done;
    manager = client_bind(&connection,
                                 "treeland_prelaunch_splash_manager_v2",
                                 &treeland_prelaunch_splash_manager_v2_interface,
                                 2);
    if (!manager)
        goto done;

    splash = treeland_prelaunch_splash_manager_v2_create_splash(
        manager, splash_app_id, "protocol-instance", "org.deepin.Sandbox", NULL);
    if (!splash || wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(prelaunch_splash_desktop_wait_for_creation,
                                        &creation_observed)
        || !creation_observed
        || !invoke_on_server_thread(prelaunch_splash_desktop_read_state, &state)
        || !state_has_production_splash(&state)) {
        goto done;
    }

    treeland_prelaunch_splash_v2_destroy(splash);
    splash = NULL;
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(prelaunch_splash_desktop_wait_for_destruction,
                                        &destruction_observed)
        || !destruction_observed
        || !invoke_on_server_thread(prelaunch_splash_desktop_read_state, &state)
        || !state.wrapper_destroyed || state.wrapper_in_workspace) {
        goto done;
    }

    result = 0;
done:
    if (result != 0) {
        fprintf(stderr,
                "prelaunch splash desktop failed: output=%d created=%d workspace=%d splash=%d qml=%d "
                "destroyed=%d size=%dx%d app=%s\n",
                state.output_ready,
                state.wrapper_created,
                state.wrapper_in_workspace,
                state.wrapper_is_splash,
                state.wrapper_has_qml_item,
                state.wrapper_destroyed,
                state.wrapper_width,
                state.wrapper_height,
                state.app_id);
    }
    if (splash)
        treeland_prelaunch_splash_v2_destroy(splash);
    if (manager)
        treeland_prelaunch_splash_manager_v2_destroy(manager);
    client_disconnect(&connection);
    return result;
}
