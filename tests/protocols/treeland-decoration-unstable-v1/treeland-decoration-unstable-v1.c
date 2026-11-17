// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-decoration-unstable-v1.h"
#include "xdg-toplevel-client.h"
#include "treeland-decoration-unstable-v1-client-protocol.h"

#include <stdio.h>

// The protocol reserves values >= 10000 as "use the compositor default"
// (implementer-defined constant; see treeland-decoration-unstable-v1.xml).
enum { DEFAULT_SENTINEL = 10000 };

static int read_state(struct client_connection *connection, struct decoration_server_state *state)
{
    if (wl_display_roundtrip(connection->display) < 0)
        return 0;
    if (!invoke_on_server_thread(decoration_read_server_state, state)) {
        fprintf(stderr, "decoration: server state read failed\n");
        return 0;
    }
    return 1;
}

#define REQUIRE(cond, ...) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "decoration: " __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            return 1; \
        } \
    } while (0)

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    if (!client_connect(&connection, socket_name)
        || !xdg_toplevel_client_create(&connection, &toplevel))
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

    struct decoration_server_state state = { 0 };

    // --- Corner radius: override then fall back to the compositor default. ---
    treeland_decoration_context_v1_set_corner_radius(context, 25);
    REQUIRE(read_state(&connection, &state) && state.valid, "radius read failed");
    REQUIRE(state.radius == 25.0, "corner radius override not applied (radius=%g)", state.radius);

    treeland_decoration_context_v1_set_corner_radius(context, DEFAULT_SENTINEL);
    REQUIRE(read_state(&connection, &state) && state.valid, "radius default read failed");
    // Compositor default = DConfig windowRadius (12 in the test environment).
    REQUIRE(state.radius == 12.0, "corner radius sentinel did not restore default (radius=%g)",
            state.radius);

    // --- Shadow: full override, then per-argument sentinel fallback. ---
    treeland_decoration_context_v1_set_shadow(context, 5, 3, 4, 10, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "shadow read failed");
    REQUIRE(state.shadow_blur == 5.0 && state.shadow_offset_x == 3.0
                && state.shadow_offset_y == 4.0 && state.shadow_r == 10 && state.shadow_g == 20
                && state.shadow_b == 30 && state.shadow_a == 40 && state.shadow_visible == 1,
            "shadow override not applied (blur=%g off=%g,%g rgba=%d,%d,%d,%d visible=%d)",
            state.shadow_blur, state.shadow_offset_x, state.shadow_offset_y, state.shadow_r,
            state.shadow_g, state.shadow_b, state.shadow_a, state.shadow_visible);

    // Only the blur radius is a sentinel: radius falls back to 40, the rest is kept.
    treeland_decoration_context_v1_set_shadow(context, DEFAULT_SENTINEL, 3, 4, 10, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "shadow radius sentinel read failed");
    REQUIRE(state.shadow_blur == 40.0 && state.shadow_offset_x == 3.0
                && state.shadow_offset_y == 4.0 && state.shadow_r == 10 && state.shadow_g == 20
                && state.shadow_b == 30 && state.shadow_a == 40 && state.shadow_visible == 1,
            "shadow radius sentinel did not restore default blur (blur=%g off=%g,%g)",
            state.shadow_blur, state.shadow_offset_x, state.shadow_offset_y);

    // Only offset_x is a sentinel: offset_x falls back to 0, the rest is kept.
    treeland_decoration_context_v1_set_shadow(context, 5, DEFAULT_SENTINEL, 4, 10, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "shadow offset_x sentinel read failed");
    REQUIRE(state.shadow_blur == 5.0 && state.shadow_offset_x == 0.0
                && state.shadow_offset_y == 4.0 && state.shadow_r == 10 && state.shadow_g == 20
                && state.shadow_b == 30 && state.shadow_a == 40 && state.shadow_visible == 1,
            "shadow offset_x sentinel did not restore default offset (blur=%g off=%g,%g)",
            state.shadow_blur, state.shadow_offset_x, state.shadow_offset_y);

    // Only offset_y is a sentinel: offset_y falls back to 10, the rest is kept.
    treeland_decoration_context_v1_set_shadow(context, 5, 3, DEFAULT_SENTINEL, 10, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "shadow offset_y sentinel read failed");
    REQUIRE(state.shadow_blur == 5.0 && state.shadow_offset_x == 3.0
                && state.shadow_offset_y == 10.0 && state.shadow_r == 10 && state.shadow_g == 20
                && state.shadow_b == 30 && state.shadow_a == 40 && state.shadow_visible == 1,
            "shadow offset_y sentinel did not restore default offset (blur=%g off=%g,%g)",
            state.shadow_blur, state.shadow_offset_x, state.shadow_offset_y);

    // A sentinel in any color component selects the default shadow color (all
    // other color components ignored); the non-color arguments are kept.
    treeland_decoration_context_v1_set_shadow(context, 5, 3, 4, DEFAULT_SENTINEL, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "shadow color sentinel read failed");
    REQUIRE(state.shadow_blur == 5.0 && state.shadow_offset_x == 3.0
                && state.shadow_offset_y == 4.0 && state.shadow_r == 0 && state.shadow_g == 0
                && state.shadow_b == 0 && state.shadow_a == 102 && state.shadow_visible == 1,
            "shadow color sentinel did not restore default color (rgba=%d,%d,%d,%d)",
            state.shadow_r, state.shadow_g, state.shadow_b, state.shadow_a);

    // --- Border: full override, then per-argument sentinel fallback. ---
    treeland_decoration_context_v1_set_border(context, 3, 10, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "border read failed");
    REQUIRE(state.border_width == 3.0 && state.border_r == 10 && state.border_g == 20
                && state.border_b == 30 && state.border_a == 40 && state.border_visible == 1,
            "border override not applied (width=%g rgba=%d,%d,%d,%d visible=%d)",
            state.border_width, state.border_r, state.border_g, state.border_b, state.border_a,
            state.border_visible);

    // Width sentinel: width falls back to 1, color is kept.
    treeland_decoration_context_v1_set_border(context, DEFAULT_SENTINEL, 10, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "border width sentinel read failed");
    REQUIRE(state.border_width == 1.0 && state.border_r == 10 && state.border_g == 20
                && state.border_b == 30 && state.border_a == 40 && state.border_visible == 1,
            "border width sentinel did not restore default width (width=%g)", state.border_width);

    // Color sentinel: default border color; width is kept.
    treeland_decoration_context_v1_set_border(context, 3, DEFAULT_SENTINEL, 20, 30, 40);
    REQUIRE(read_state(&connection, &state) && state.valid, "border color sentinel read failed");
    REQUIRE(state.border_width == 3.0 && state.border_r == 0 && state.border_g == 0
                && state.border_b == 0 && state.border_a == 26 && state.border_visible == 1,
            "border color sentinel did not restore default color (rgba=%d,%d,%d,%d)",
            state.border_r, state.border_g, state.border_b, state.border_a);

    treeland_decoration_context_v1_destroy(context);
    treeland_decoration_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;
}
