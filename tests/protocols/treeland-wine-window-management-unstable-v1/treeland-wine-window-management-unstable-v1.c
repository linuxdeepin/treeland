// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-wine-window-management-unstable-v1.h"
#include "server-bridge-api.h"
#include "treeland-wine-window-management-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

static uint32_t received_window_id;
static int received_window_id_count;
static int32_t last_pos_x;
static int32_t last_pos_y;
static uint32_t last_pos_serial;
static int configure_position_count;
static uint32_t last_topmost;
static int configure_stacking_count;

static void handle_window_id(void *data,
                            struct treeland_wine_window_control_v1 *control,
                            uint32_t id)
{
    (void)data;
    (void)control;
    received_window_id = id;
    ++received_window_id_count;
}

static void handle_configure_position(void *data,
                                      struct treeland_wine_window_control_v1 *control,
                                      int32_t x,
                                      int32_t y,
                                      uint32_t serial)
{
    (void)data;
    (void)control;
    last_pos_x = x;
    last_pos_y = y;
    last_pos_serial = serial;
    ++configure_position_count;
}

static void handle_configure_stacking(void *data,
                                      struct treeland_wine_window_control_v1 *control,
                                      uint32_t topmost)
{
    (void)data;
    (void)control;
    last_topmost = topmost;
    ++configure_stacking_count;
}

static const struct treeland_wine_window_control_v1_listener control_listener = {
    .window_id = handle_window_id,
    .configure_position = handle_configure_position,
    .configure_stacking = handle_configure_stacking,
};

static int read_state(struct wine_wm_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(wine_wm_read_state, state);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = {0};
    struct treeland_wine_window_manager_v1 *manager = NULL;
    struct treeland_wine_window_control_v1 *control = NULL;
    struct wine_wm_state state = {0};
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;
    if (!xdg_toplevel_client_create(&connection, &toplevel))
        goto failed;
    stage = 1;

    if (!read_state(&state) || !state.wrapper_created)
        goto failed;

    manager = client_bind(&connection,
                                 "treeland_wine_window_manager_v1",
                                 &treeland_wine_window_manager_v1_interface,
                                 1);
    if (!manager)
        goto failed;
    stage = 2;

    control = treeland_wine_window_manager_v1_get_window_control(manager, toplevel.toplevel);
    treeland_wine_window_control_v1_add_listener(control, &control_listener, NULL);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 3;
    if (received_window_id_count != 1 || received_window_id == 0)
        goto failed;
    if (configure_position_count < 1 || last_pos_serial != 0)
        goto failed;
    if (configure_stacking_count < 1 || last_topmost != 0)
        goto failed;

    if (!read_state(&state) || !state.wrapper_created)
        goto failed;
    if (state.z != 0 || state.effective_always_on_top != 0)
        goto failed;
    if (state.parent_item_count < 1)
        goto failed;
    stage = 4;

    treeland_wine_window_control_v1_set_position(control, 100, 200, 1);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 4;
    if (last_pos_serial != 1)
        goto failed;

    if (!read_state(&state) || state.x != 100 || state.y != 200)
        goto failed;
    stage = 5;

    treeland_wine_window_control_v1_set_z_order(control,
                                                  TREELAND_WINE_WINDOW_CONTROL_V1_Z_ORDER_OP_HWND_TOPMOST,
                                                  0);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 5;
    if (last_topmost != 1)
        goto failed;

    if (!read_state(&state) || state.z != 1 || state.effective_always_on_top != 1)
        goto failed;
    stage = 6;

    treeland_wine_window_control_v1_set_z_order(control,
                                                  TREELAND_WINE_WINDOW_CONTROL_V1_Z_ORDER_OP_HWND_NOTOPMOST,
                                                  0);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 6;
    if (last_topmost != 0)
        goto failed;

    if (!read_state(&state) || state.z != 0 || state.effective_always_on_top != 0)
        goto failed;

    treeland_wine_window_control_v1_destroy(control);
    treeland_wine_window_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "wine-window-management failure at stage %d: window_id=%u(%d) pos=(%d,%d) serial=%u(%d) "
            "topmost=%u(%d) z=%d effective_top=%d parent_items=%d\n",
            stage, received_window_id, received_window_id_count,
            last_pos_x, last_pos_y, last_pos_serial, configure_position_count,
            last_topmost, configure_stacking_count,
            state.z, state.effective_always_on_top, state.parent_item_count);
    if (control)
        treeland_wine_window_control_v1_destroy(control);
    if (manager)
        treeland_wine_window_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 1;
}
