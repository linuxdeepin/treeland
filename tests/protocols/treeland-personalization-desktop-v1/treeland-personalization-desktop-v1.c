// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-personalization-desktop-v1.h"
#include "server-bridge-api.h"
#include "treeland-personalization-manager-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void personalization_desktop_read_state(void *data);

static int state_matches(const struct personalization_desktop_state *state)
{
    return state->output_ready
           && state->wrapper_created
           && state->wrapper_in_workspace
           && state->background_type == TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_BLUR
           && state->corner_radius == 12
           && state->blur
           && state->no_titlebar
           && state->wrapper_no_titlebar
           && state->shadow_radius == 8
           && state->shadow_offset_x == 2
           && state->shadow_offset_y == 3
           && state->shadow_red == 10
           && state->shadow_green == 20
           && state->shadow_blue == 30
           && state->shadow_alpha == 40
           && state->border_width == 2
           && state->border_red == 100
           && state->border_green == 150
           && state->border_blue == 200
           && state->border_alpha == 255;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct treeland_personalization_manager_v1 *manager = NULL;
    struct treeland_personalization_window_context_v1 *context = NULL;
    struct personalization_desktop_state state = { 0 };
    int stage = 0;
    if (!client_connect(&connection, socket_name))
        return 1;
    stage = 1;
    manager = client_bind(&connection,
                                 "treeland_personalization_manager_v1",
                                 &treeland_personalization_manager_v1_interface,
                                 1);
    if (!manager)
        goto failed;
    stage = 2;
    if (!xdg_toplevel_client_create(&connection, &toplevel))
        goto failed;
    stage = 3;

    context = treeland_personalization_manager_v1_get_window_context(manager, toplevel.surface);
    if (!context)
        goto failed;
    stage = 4;
    treeland_personalization_window_context_v1_set_blend_mode(
        context, TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_BLEND_MODE_BLUR);
    treeland_personalization_window_context_v1_set_round_corner_radius(context, 12);
    treeland_personalization_window_context_v1_set_shadow(context, 8, 2, 3, 10, 20, 30, 40);
    treeland_personalization_window_context_v1_set_border(context, 2, 100, 150, 200, 255);
    treeland_personalization_window_context_v1_set_titlebar(
        context, TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_ENABLE_MODE_DISABLE);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 5;

    memset(&state, 0, sizeof(state));
    if (!invoke_on_server_thread(personalization_desktop_read_state, &state))
        goto failed;
    stage = 6;
    if (!state_matches(&state))
        goto failed;

    treeland_personalization_window_context_v1_destroy(context);
    xdg_toplevel_client_destroy(&toplevel);
    treeland_personalization_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "personalization failure at stage %d: output=%d wrapper=%d workspace=%d type=%d radius=%d blur=%d "
            "attached=%d context-titlebar=%d wrapper-titlebar=%d shadow=(%d,%d,%d,%d,%d,%d,%d) "
            "border=(%d,%d,%d,%d,%d)\n",
            stage, state.output_ready, state.wrapper_created, state.wrapper_in_workspace,
            state.background_type, state.corner_radius, state.blur, state.personalization_attached, state.no_titlebar,
            state.wrapper_no_titlebar, state.shadow_radius, state.shadow_offset_x,
            state.shadow_offset_y, state.shadow_red, state.shadow_green, state.shadow_blue,
            state.shadow_alpha, state.border_width, state.border_red, state.border_green,
            state.border_blue, state.border_alpha);
    if (context) treeland_personalization_window_context_v1_destroy(context);
    xdg_toplevel_client_destroy(&toplevel);
    if (manager) treeland_personalization_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return 1;
}
