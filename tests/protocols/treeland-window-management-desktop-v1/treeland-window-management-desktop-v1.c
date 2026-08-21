// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-window-management-desktop-v1.h"
#include "server-bridge-api.h"
#include "treeland-window-management-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void window_management_desktop_read_state(void *data);
extern void window_management_desktop_wait_visible(void *data);

struct window_management_client {
    unsigned int last_state;
    int state_events;
};

static void show_desktop(void *data,
                         struct treeland_window_management_v1 *manager,
                         uint32_t state)
{
    (void)manager;
    struct window_management_client *client = data;
    client->last_state = state;
    ++client->state_events;
}

static const struct treeland_window_management_v1_listener manager_listener = {
    .show_desktop = show_desktop,
};

static int read_state(struct window_management_desktop_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(window_management_desktop_read_state, state);
}

static int wait_visible(int visible)
{
    struct window_management_desktop_visibility_wait wait = {
        .visible = visible,
    };
    return invoke_on_server_thread(window_management_desktop_wait_visible, &wait)
        && wait.reached;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct treeland_window_management_v1 *manager = NULL;
    struct window_management_client client = { 0 };
    struct window_management_desktop_state state = { 0 };
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;
    manager = client_bind(&connection,
                                 "treeland_window_management_v1",
                                 &treeland_window_management_v1_interface,
                                 1);
    if (!manager)
        goto failed;
    treeland_window_management_v1_add_listener(manager, &manager_listener, &client);
    stage = 1;
    if (!xdg_toplevel_client_create(&connection, &toplevel))
        goto failed;
    stage = 2;
    if (!read_state(&state)
        || !state.wrapper_created
        || !state.wrapper_in_workspace
        || !state.wrapper_in_paint_order
        || !state.wrapper_visible
        || state.wrapper_minimized
        || state.desktop_state != TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL)
        goto failed;

    treeland_window_management_v1_set_desktop(
        manager, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 3;
    if (!wait_visible(0)
        || !read_state(&state)
        || client.last_state != TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW
        || !client.state_events
        || state.desktop_state != TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW
        || state.wrapper_visible
        || state.wrapper_minimized)
        goto failed;

    treeland_window_management_v1_set_desktop(
        manager, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 4;
    if (!wait_visible(1)
        || !read_state(&state)
        || client.last_state != TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL
        || state.desktop_state != TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL
        || !state.wrapper_visible
        || state.wrapper_minimized)
        goto failed;

    xdg_toplevel_client_destroy(&toplevel);
    treeland_window_management_v1_destroy(manager);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "window-management desktop failure at stage %d: wrapper=%d workspace=%d paint-order=%d visible=%d "
            "minimized=%d state=%u event=%u\n",
            stage, state.wrapper_created, state.wrapper_in_workspace, state.wrapper_in_paint_order, state.wrapper_visible,
            state.wrapper_minimized, state.desktop_state, client.last_state);
    xdg_toplevel_client_destroy(&toplevel);
    if (manager)
        treeland_window_management_v1_destroy(manager);
    client_disconnect(&connection);
    return 1;
}
