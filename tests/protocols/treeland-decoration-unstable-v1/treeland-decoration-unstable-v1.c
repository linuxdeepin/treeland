// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-decoration-unstable-v1.h"
#include "xdg-toplevel-client.h"
#include "treeland-decoration-unstable-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <stdio.h>

struct decoration_mode_state {
    uint32_t mode;
    int received;
};

static void decoration_configure(void *data,
                                 struct zxdg_toplevel_decoration_v1 *decoration,
                                 uint32_t mode)
{
    (void)decoration;
    struct decoration_mode_state *state = data;
    state->mode = mode;
    state->received = 1;
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
    .configure = decoration_configure,
};

// The compositor answers set_mode with a configure; wait for it before the next step.
static struct zxdg_toplevel_decoration_v1 *request_decoration(
    struct client_connection *connection,
    struct zxdg_decoration_manager_v1 *manager,
    struct xdg_toplevel *toplevel,
    uint32_t mode,
    struct decoration_mode_state *state)
{
    struct zxdg_toplevel_decoration_v1 *decoration =
        zxdg_decoration_manager_v1_get_toplevel_decoration(manager, toplevel);
    if (!decoration)
        return NULL;
    zxdg_toplevel_decoration_v1_add_listener(decoration, &decoration_listener, state);
    zxdg_toplevel_decoration_v1_set_mode(decoration, mode);
    for (int i = 0; i < 32 && !state->received; ++i) {
        if (wl_display_roundtrip(connection->display) < 0)
            break;
    }
    return decoration;
}

static int read_server_state(struct decoration_server_state *state)
{
    if (!invoke_on_server_thread(decoration_read_server_state, state) || !state->valid) {
        fprintf(stderr, "decoration: server state unavailable\n");
        return 1;
    }
    return 0;
}

static int expect_effects(double radius, int shadow_visible, int border_visible, const char *step)
{
    struct decoration_server_state state = { 0 };
    if (read_server_state(&state))
        return 1;
    const int radius_ok = state.radius == radius
        && (radius > 0 || (state.frame_radius == 0 && state.presentation_radius == 0));
    if (radius_ok && state.shadow_visible == shadow_visible && state.border_visible == border_visible
        && (!shadow_visible || state.shadow_radius == 24)
        && (!border_visible || state.border_width == 2))
        return 0;
    fprintf(stderr,
            "decoration: %s: radius=%g (frame=%g presentation=%g) shadow=%d (%g px) border=%d (%g px)\n",
            step, state.radius, state.frame_radius, state.presentation_radius,
            state.shadow_visible, state.shadow_radius, state.border_visible, state.border_width);
    return 1;
}

// A maximized window keeps its frame on the rect: only the height gives up the title bar.
static int expect_maximized_configure(struct client_connection *connection,
                                      struct xdg_toplevel_client *toplevel,
                                      const char *step)
{
    // Bounded wait: a compositor that never corrects the size must fail, not hang.
    for (int i = 0; i < 32; ++i) {
        struct decoration_server_state state = { 0 };
        if (read_server_state(&state))
            return 1;
        if (toplevel->configure_width == state.maximized_width
            && toplevel->configure_height == state.maximized_height - state.titlebar_height)
            return 0;
        if (wl_display_roundtrip(connection->display) < 0)
            return 1;
    }
    struct decoration_server_state state = { 0 };
    if (read_server_state(&state))
        return 1;
    fprintf(stderr,
            "decoration: configure %dx%d after %s, expected %dx%d "
            "(maximized %dx%d, title bar %d)\n",
            toplevel->configure_width, toplevel->configure_height, step,
            state.maximized_width, state.maximized_height - state.titlebar_height,
            state.maximized_width, state.maximized_height, state.titlebar_height);
    return 1;
}

// Hide or show the title bar and wait until the server side really did it.
static int set_titlebar_mode(struct client_connection *connection,
                            struct treeland_decoration_context_v1 *context,
                            uint32_t mode,
                            int32_t expectedHeight)
{
    treeland_decoration_context_v1_set_titlebar_mode(context, mode);
    if (wl_display_roundtrip(connection->display) < 0)
        return 1;
    for (int i = 0; i < 32; ++i) {
        struct decoration_server_state state = { 0 };
        if (read_server_state(&state))
            return 1;
        if ((expectedHeight == 0 && state.titlebar_height == 0)
            || (expectedHeight > 0 && state.titlebar_height == expectedHeight))
            return 0;
        if (wl_display_roundtrip(connection->display) < 0)
            return 1;
    }
    fprintf(stderr, "decoration: title bar padding did not become %d\n", expectedHeight);
    return 1;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    if (!client_connect(&connection, socket_name)
        || !xdg_toplevel_client_create_pending(&connection, &toplevel))
        return 1;
    struct treeland_decoration_manager_v1 *manager = client_bind(&connection,
        "treeland_decoration_manager_v1", &treeland_decoration_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "decoration: manager missing\n");
        return 1;
    }
    struct treeland_decoration_context_v1 *context =
        treeland_decoration_manager_v1_get_decoration_context(manager, toplevel.surface);
    if (!context) {
        fprintf(stderr, "decoration: context creation failed\n");
        return 1;
    }
    treeland_decoration_context_v1_set_corner_radius(context, 12);
    if (wl_display_roundtrip(connection.display) < 0)
        return 1;
    struct decoration_server_state state = { 0 };
    if (read_server_state(&state))
        return 1;
    if (state.radius != 12.0) {
        fprintf(stderr, "decoration: corner radius was not applied (radius=%g)\n", state.radius);
        return 1;
    }

    struct decoration_effect_switches switches = { 0, 0, 0 };
    if (!invoke_on_server_thread(decoration_set_effect_switches, &switches)
        || expect_effects(0, 0, 0, "DConfig disabled decorations"))
        return 1;

    treeland_decoration_context_v1_set_corner_radius(context, 16);
    treeland_decoration_context_v1_set_shadow(context, 24, 0, 8, 0, 0, 0, 128);
    treeland_decoration_context_v1_set_border(context, 2, 0, 0, 0, 128);
    if (wl_display_roundtrip(connection.display) < 0
        || expect_effects(0, 0, 0, "client requests while DConfig disabled decorations"))
        return 1;

    // Toggle each switch independently to catch any cross-wiring.
    switches = (struct decoration_effect_switches){ 1, 0, 0 };
    if (!invoke_on_server_thread(decoration_set_effect_switches, &switches)
        || expect_effects(16, 0, 0, "DConfig enabled corner radius only"))
        return 1;

    switches = (struct decoration_effect_switches){ 0, 1, 0 };
    if (!invoke_on_server_thread(decoration_set_effect_switches, &switches)
        || expect_effects(0, 1, 0, "DConfig enabled shadow only"))
        return 1;

    switches = (struct decoration_effect_switches){ 0, 0, 1 };
    if (!invoke_on_server_thread(decoration_set_effect_switches, &switches)
        || expect_effects(0, 0, 1, "DConfig enabled border only"))
        return 1;

    switches = (struct decoration_effect_switches){ 1, 1, 1 };
    if (!invoke_on_server_thread(decoration_set_effect_switches, &switches)
        || expect_effects(16, 1, 1, "DConfig re-enabled decorations"))
        return 1;

    // 0 means square corners, not "fall back to the compositor default radius".
    treeland_decoration_context_v1_set_corner_radius(context, 0);
    if (wl_display_roundtrip(connection.display) < 0)
        return 1;
    state = (struct decoration_server_state){ 0 };
    if (read_server_state(&state))
        return 1;
    if (state.radius != 0.0) {
        fprintf(stderr, "decoration: radius 0 was not applied (radius=%g)\n", state.radius);
        return 1;
    }

    // Restore maximized: the bar only lands once the toplevel initializes, after a first apply.
    struct zxdg_decoration_manager_v1 *decorationManager = client_bind(&connection,
        "zxdg_decoration_manager_v1", &zxdg_decoration_manager_v1_interface, 1);
    if (!decorationManager) {
        fprintf(stderr, "decoration: xdg-decoration manager missing\n");
        return 1;
    }
    xdg_toplevel_set_maximized(toplevel.toplevel);
    struct decoration_mode_state mode = { 0 };
    struct zxdg_toplevel_decoration_v1 *decoration =
        request_decoration(&connection, decorationManager, toplevel.toplevel,
                           ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE, &mode);
    if (!decoration || mode.mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
        fprintf(stderr, "decoration: server-side decoration not negotiated (received=%d mode=%u)\n",
                mode.received, mode.mode);
        return 1;
    }
    if (!xdg_toplevel_client_complete_map(&connection, &toplevel))
        return 1;
    if (read_server_state(&state))
        return 1;
    if (state.titlebar_height <= 0) {
        fprintf(stderr, "decoration: server-side decoration drew no title bar\n");
        return 1;
    }
    if (expect_maximized_configure(&connection, &toplevel, "the title bar appeared"))
        return 1;

    // Hiding the title bar gives that space back to the client area.
    const int32_t titlebar_height = state.titlebar_height;
    if (set_titlebar_mode(&connection, context,
                          TREELAND_DECORATION_CONTEXT_V1_TITLEBAR_MODE_HIDE, 0))
        return 1;
    if (expect_maximized_configure(&connection, &toplevel, "the title bar went away"))
        return 1;

    // ...and showing it again takes that space back.
    if (set_titlebar_mode(&connection, context,
                          TREELAND_DECORATION_CONTEXT_V1_TITLEBAR_MODE_SHOW, titlebar_height))
        return 1;
    if (expect_maximized_configure(&connection, &toplevel, "the title bar came back"))
        return 1;

    zxdg_toplevel_decoration_v1_destroy(decoration);
    zxdg_decoration_manager_v1_destroy(decorationManager);
    treeland_decoration_context_v1_destroy(context);
    treeland_decoration_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;
}
