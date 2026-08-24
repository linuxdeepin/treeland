// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-wine-window-state-unstable-v1.h"
#include "server-bridge-api.h"
#include "treeland-wine-window-state-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void wine_ws_read_state(void *data);
extern void wine_ws_minimize_wrapper(void *);
extern void wine_ws_unminimize_wrapper(void *);
extern void wine_ws_set_attention(void *);
extern void wine_ws_clear_attention(void *);

struct wine_ws_client {
    uint32_t last_state;
    int state_events;
};

static void handle_state_changed(void *data,
                                  struct treeland_wine_window_state_v1 *state,
                                  uint32_t ws_state)
{
    (void)state;
    struct wine_ws_client *client = data;
    client->last_state = ws_state;
    ++client->state_events;
}

static void handle_activate_denied(void *data,
                                    struct treeland_wine_window_state_v1 *state,
                                    uint32_t serial)
{
    (void)data;
    (void)state;
    (void)serial;
}

static const struct treeland_wine_window_state_v1_listener state_listener = {
    .state_changed = handle_state_changed,
    .activate_denied = handle_activate_denied,
};

static int read_state(struct wine_ws_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(wine_ws_read_state, state);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct treeland_wine_window_state_manager_v1 *manager = NULL;
    struct treeland_wine_window_state_v1 *ws = NULL;
    struct wine_ws_client client = { 0 };
    struct wine_ws_state state = { 0 };
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;
    stage = 1;

    if (!xdg_toplevel_client_create(&connection, &toplevel))
        goto failed;
    stage = 2;

    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    if (!read_state(&state) || !state.wrapper_created)
        goto failed;
    stage = 3;

    manager = client_bind(&connection,
                                 "treeland_wine_window_state_manager_v1",
                                 &treeland_wine_window_state_manager_v1_interface,
                                 1);
    if (!manager)
        goto failed;
    stage = 4;

    client.state_events = 0;
    client.last_state = 0;
    ws = treeland_wine_window_state_manager_v1_get_window_state(manager, toplevel.toplevel);
    treeland_wine_window_state_v1_add_listener(ws, &state_listener, &client);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    if (client.state_events < 1 || client.last_state != 0)
        goto failed;

    if (!read_state(&state) || !state.visible)
        goto failed;
    stage = 5;

    client.state_events = 0;
    client.last_state = 0;
    invoke_on_server_thread(wine_ws_minimize_wrapper, NULL);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    if (client.state_events < 1
        || !(client.last_state & TREELAND_WINE_WINDOW_STATE_V1_STATE_MINIMIZED))
        goto failed;

    if (!read_state(&state) || state.minimized != 1 || state.visible != 0)
        goto failed;
    stage = 6;

    client.state_events = 0;
    client.last_state = 0;
    treeland_wine_window_state_v1_set_attention(ws, 0, 0);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    if (client.state_events < 1
        || !(client.last_state & (TREELAND_WINE_WINDOW_STATE_V1_STATE_MINIMIZED
                                       | TREELAND_WINE_WINDOW_STATE_V1_STATE_ATTENTION)))
        goto failed;
    if (!read_state(&state) || state.attention != 1 || state.visible != 0)
        goto failed;
    stage = 7;

    client.state_events = 0;
    client.last_state = 0;
    treeland_wine_window_state_v1_clear_attention(ws);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    if (client.state_events < 1
        || (client.last_state & TREELAND_WINE_WINDOW_STATE_V1_STATE_ATTENTION))
        goto failed;
    if (!read_state(&state) || state.attention != 0 || state.visible != 0)
        goto failed;
    stage = 8;

    client.state_events = 0;
    client.last_state = 0;
    treeland_wine_window_state_v1_unminimize(ws);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    if (client.state_events < 1
        || (client.last_state & TREELAND_WINE_WINDOW_STATE_V1_STATE_MINIMIZED))
        goto failed;

    if (!read_state(&state) || state.minimized != 0 || state.visible != 1)
        goto failed;

    treeland_wine_window_state_v1_destroy(ws);
    treeland_wine_window_state_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "wine-window-state failure at stage %d: state=0x%x events=%d "
            "wrapper=%d minimized=%d attention=%d visible=%d\n",
            stage, client.last_state, client.state_events,
            state.wrapper_created, state.minimized, state.attention, state.visible);
    if (ws)
        treeland_wine_window_state_v1_destroy(ws);
    if (manager)
        treeland_wine_window_state_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 1;
}
