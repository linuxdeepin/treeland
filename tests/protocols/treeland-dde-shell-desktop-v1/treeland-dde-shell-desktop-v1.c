// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-dde-shell-desktop-v1.h"
#include "server-bridge-api.h"
#include "treeland-dde-shell-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void dde_desktop_read_state(void *data);

struct dde_desktop_client {
    struct treeland_dde_shell_manager_v1 *manager;
    struct treeland_dde_shell_surface_v1 *shell_surface;
};

static int setup_dde_surface(struct wl_surface *surface, void *data)
{
    struct dde_desktop_client *client = data;
    client->shell_surface = treeland_dde_shell_manager_v1_get_shell_surface(client->manager, surface);
    if (!client->shell_surface)
        return 0;
    treeland_dde_shell_surface_v1_set_surface_position(client->shell_surface, 42, 24);
    treeland_dde_shell_surface_v1_set_role(client->shell_surface,
                                           TREELAND_DDE_SHELL_SURFACE_V1_ROLE_OVERLAY);
    treeland_dde_shell_surface_v1_set_auto_placement(client->shell_surface, 37);
    treeland_dde_shell_surface_v1_set_skip_switcher(client->shell_surface, 1);
    treeland_dde_shell_surface_v1_set_skip_dock_preview(client->shell_surface, 1);
    treeland_dde_shell_surface_v1_set_skip_muti_task_view(client->shell_surface, 1);
    treeland_dde_shell_surface_v1_set_accept_keyboard_focus(client->shell_surface, 0);
    return 1;
}

static int state_matches(struct dde_desktop_state *state, int expected_skip_dock_preview)
{
    return state->output_ready
           && state->wrapper_created
           && state->wrapper_in_workspace
           && state->is_dde_shell_surface
           && state->role_overlay
           && state->position_x == 42
           && state->position_y == 24
           && state->auto_placement == 37
           && state->skip_switcher
           && state->skip_dock_preview == expected_skip_dock_preview
           && state->skip_multitask_view
           && !state->accept_keyboard_focus;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct dde_desktop_client client = { 0 };
    struct dde_desktop_state state;
    if (!client_connect(&connection, socket_name))
        return 1;
    client.manager = client_bind(&connection,
                                        "treeland_dde_shell_manager_v1",
                                        &treeland_dde_shell_manager_v1_interface,
                                        1);
    if (!client.manager
        || !xdg_toplevel_client_create_with_surface_setup(&connection,
                                                                  &toplevel,
                                                                  setup_dde_surface,
                                                                  &client))
        goto failed;

    memset(&state, 0, sizeof(state));
    if (!invoke_on_server_thread(dde_desktop_read_state, &state)
        || !state_matches(&state, 1))
        goto failed;

    treeland_dde_shell_surface_v1_set_skip_dock_preview(client.shell_surface, 0);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    memset(&state, 0, sizeof(state));
    if (!invoke_on_server_thread(dde_desktop_read_state, &state)
        || !state_matches(&state, 0))
        goto failed;

    xdg_toplevel_client_destroy(&toplevel);
    treeland_dde_shell_surface_v1_destroy(client.shell_surface);
    treeland_dde_shell_manager_v1_destroy(client.manager);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr, "DDE shell requests did not reach the production SurfaceWrapper\n");
    xdg_toplevel_client_destroy(&toplevel);
    if (client.shell_surface) treeland_dde_shell_surface_v1_destroy(client.shell_surface);
    if (client.manager) treeland_dde_shell_manager_v1_destroy(client.manager);
    client_disconnect(&connection);
    return 1;
}
