// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-show-desktop-desktop-v1.h"
#include "server-bridge-api.h"
#include "treeland-show-desktop-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void show_desktop_desktop_read_state(void *data);
extern void show_desktop_desktop_wait_visible(void *data);

struct show_desktop_client {
    unsigned int last_state;
    int state_events;
};

static void show_desktop_state(void *data,
                         struct treeland_show_desktop_v1 *manager,
                         uint32_t state)
{
    (void)manager;
    struct show_desktop_client *client = data;
    client->last_state = state;
    ++client->state_events;
}

static const struct treeland_show_desktop_v1_listener manager_listener = {
    .show_desktop_state = show_desktop_state,
};

static int read_state(struct show_desktop_desktop_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(show_desktop_desktop_read_state, state);
}

static int wait_visible(int visible)
{
    struct show_desktop_desktop_visibility_wait wait = {
        .visible = visible,
    };
    return invoke_on_server_thread(show_desktop_desktop_wait_visible, &wait)
        && wait.reached;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct treeland_show_desktop_v1 *manager = NULL;
    struct show_desktop_client client = { 0 };
    struct show_desktop_desktop_state state = { 0 };
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;
    manager = client_bind(&connection,
                                 "treeland_show_desktop_v1",
                                 &treeland_show_desktop_v1_interface,
                                 1);
    if (!manager)
        goto failed;
    treeland_show_desktop_v1_add_listener(manager, &manager_listener, &client);
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
        || state.desktop_state != TREELAND_SHOW_DESKTOP_V1_STATE_NORMAL)
        goto failed;

    treeland_show_desktop_v1_set_show_desktop_state(
        manager, TREELAND_SHOW_DESKTOP_V1_STATE_SHOW);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 3;
    if (!wait_visible(0)
        || !read_state(&state)
        || client.last_state != TREELAND_SHOW_DESKTOP_V1_STATE_SHOW
        || !client.state_events
        || state.desktop_state != TREELAND_SHOW_DESKTOP_V1_STATE_SHOW
        || state.wrapper_visible
        || state.wrapper_minimized)
        goto failed;

    treeland_show_desktop_v1_set_show_desktop_state(
        manager, TREELAND_SHOW_DESKTOP_V1_STATE_NORMAL);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 4;
    if (!wait_visible(1)
        || !read_state(&state)
        || client.last_state != TREELAND_SHOW_DESKTOP_V1_STATE_NORMAL
        || state.desktop_state != TREELAND_SHOW_DESKTOP_V1_STATE_NORMAL
        || !state.wrapper_visible
        || state.wrapper_minimized)
        goto failed;

    xdg_toplevel_client_destroy(&toplevel);
    treeland_show_desktop_v1_destroy(manager);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "show-desktop desktop failure at stage %d: wrapper=%d workspace=%d paint-order=%d visible=%d "
            "minimized=%d state=%u event=%u\n",
            stage, state.wrapper_created, state.wrapper_in_workspace, state.wrapper_in_paint_order, state.wrapper_visible,
            state.wrapper_minimized, state.desktop_state, client.last_state);
    xdg_toplevel_client_destroy(&toplevel);
    if (manager)
        treeland_show_desktop_v1_destroy(manager);
    client_disconnect(&connection);
    return 1;
}
