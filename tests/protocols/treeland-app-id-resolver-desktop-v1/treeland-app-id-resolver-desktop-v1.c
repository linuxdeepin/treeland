// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "treeland-app-id-resolver-desktop-v1.h"
#include "treeland-app-id-resolver-v1-client-protocol.h"
#include "treeland-prelaunch-splash-v2-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern void app_id_resolver_desktop_wait_for_splash(void *data);
extern void app_id_resolver_desktop_read_state(void *data);

static const char *const test_app_id = "org.deepin.treeland.protocol.app-id-resolver";

struct resolver_client_state {
    int identify_received;
    uint32_t request_id;
    int pidfd;
};

static void identify_request(void *data,
                             struct treeland_app_id_resolver_v1 *resolver,
                             uint32_t request_id,
                             int32_t pidfd)
{
    (void)resolver;
    struct resolver_client_state *state = data;
    state->identify_received = 1;
    state->request_id = request_id;
    state->pidfd = pidfd;
}

static const struct treeland_app_id_resolver_v1_listener resolver_listener = {
    .identify_request = identify_request,
};

static int state_matches(const struct app_id_resolver_desktop_state *state)
{
    return state->output_ready && state->splash_created && state->wrapper_in_workspace
        && state->wrapper_converted_to_xdg && state->wrapper_app_id_matches
        && state->workspace_surface_count == 1;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct resolver_client_state resolver_state = { .pidfd = -1 };
    struct app_id_resolver_desktop_state state = { 0 };
    struct treeland_app_id_resolver_manager_v1 *resolver_manager = NULL;
    struct treeland_app_id_resolver_v1 *resolver = NULL;
    struct treeland_prelaunch_splash_manager_v2 *splash_manager = NULL;
    struct treeland_prelaunch_splash_v2 *splash = NULL;
    int splash_created = 0;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        goto done;
    resolver_manager = client_bind(&connection,
                                          "treeland_app_id_resolver_manager_v1",
                                          &treeland_app_id_resolver_manager_v1_interface,
                                          1);
    splash_manager = client_bind(&connection,
                                        "treeland_prelaunch_splash_manager_v2",
                                        &treeland_prelaunch_splash_manager_v2_interface,
                                        2);
    if (!resolver_manager || !splash_manager)
        goto done;

    resolver = treeland_app_id_resolver_manager_v1_get_resolver(resolver_manager);
    if (!resolver
        || treeland_app_id_resolver_v1_add_listener(resolver, &resolver_listener, &resolver_state))
        goto done;

    splash = treeland_prelaunch_splash_manager_v2_create_splash(
        splash_manager, test_app_id, "resolver-instance", "org.deepin.Sandbox", NULL);
    if (!splash || wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(app_id_resolver_desktop_wait_for_splash, &splash_created)
        || !splash_created)
        goto done;

    if (!xdg_toplevel_client_create_pending(&connection, &toplevel)
        || wl_display_roundtrip(connection.display) < 0
        || !resolver_state.identify_received || resolver_state.request_id == 0
        || resolver_state.pidfd < 0)
        goto done;

    treeland_app_id_resolver_v1_respond(resolver,
                                        resolver_state.request_id,
                                        test_app_id,
                                        "org.deepin.Sandbox");
    if (!xdg_toplevel_client_complete_map(&connection, &toplevel)
        || !invoke_on_server_thread(app_id_resolver_desktop_read_state, &state)
        || !state_matches(&state))
        goto done;

    result = 0;
done:
    if (result != 0) {
        fprintf(stderr,
                "app-id resolver desktop failed: identify=(received=%d id=%u pidfd=%d) "
                "wrapper=(splash=%d workspace=%d xdg=%d app-id=%d count=%d output=%d)\n",
                resolver_state.identify_received,
                resolver_state.request_id,
                resolver_state.pidfd,
                state.splash_created,
                state.wrapper_in_workspace,
                state.wrapper_converted_to_xdg,
                state.wrapper_app_id_matches,
                state.workspace_surface_count,
                state.output_ready);
    }
    if (resolver_state.pidfd >= 0)
        close(resolver_state.pidfd);
    xdg_toplevel_client_destroy(&toplevel);
    if (splash)
        treeland_prelaunch_splash_v2_destroy(splash);
    if (resolver)
        treeland_app_id_resolver_v1_destroy(resolver);
    if (splash_manager)
        treeland_prelaunch_splash_manager_v2_destroy(splash_manager);
    if (resolver_manager)
        treeland_app_id_resolver_manager_v1_destroy(resolver_manager);
    client_disconnect(&connection);
    return result;
}
