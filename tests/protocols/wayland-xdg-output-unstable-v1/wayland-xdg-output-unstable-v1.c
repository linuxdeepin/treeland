// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zxdg_output_manager_v1 global that Treeland serves through its
// WXdgOutputManager wrapper.  The test binds the first wl_output, obtains an
// xdg_output for it, and asserts that logical_position, logical_size, and
// done events are delivered.  (The headless fixture may create multiple
// outputs with different resolutions, so we only assert non-zero size.)

#include "client-connection.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct xdg_output_state {
    int has_position;
    int32_t x;
    int32_t y;
    int has_size;
    int32_t width;
    int32_t height;
    int done;
};

static void logical_position(void *data, struct zxdg_output_v1 *output,
                             int32_t x, int32_t y)
{
    (void)output;
    struct xdg_output_state *state = data;
    state->has_position = 1;
    state->x = x;
    state->y = y;
}

static void logical_size(void *data, struct zxdg_output_v1 *output,
                         int32_t width, int32_t height)
{
    (void)output;
    struct xdg_output_state *state = data;
    state->has_size = 1;
    state->width = width;
    state->height = height;
}

static void done(void *data, struct zxdg_output_v1 *output)
{
    (void)output;
    struct xdg_output_state *state = data;
    state->done = 1;
}

static void name(void *data, struct zxdg_output_v1 *output, const char *name)
{
    (void)data;
    (void)output;
    (void)name;
}

static void description(void *data, struct zxdg_output_v1 *output,
                        const char *description)
{
    (void)data;
    (void)output;
    (void)description;
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = logical_position,
    .logical_size = logical_size,
    .done = done,
    .name = name,
    .description = description,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "xdg-output: connect failed\n");
        return 1;
    }

    struct wl_output *output =
        client_bind(&conn, "wl_output", &wl_output_interface, 2);
    if (!output) {
        fprintf(stderr, "xdg-output: no wl_output global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zxdg_output_manager_v1 *manager = client_bind(
        &conn, "zxdg_output_manager_v1", &zxdg_output_manager_v1_interface, 2);
    if (!manager) {
        fprintf(stderr, "xdg-output: failed to bind zxdg_output_manager_v1\n");
        client_disconnect(&conn);
        return 1;
    }

    struct xdg_output_state state;
    memset(&state, 0, sizeof(state));
    struct zxdg_output_v1 *xdg_output =
        zxdg_output_manager_v1_get_xdg_output(manager, output);
    if (!xdg_output) {
        fprintf(stderr, "xdg-output: get_xdg_output returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }
    zxdg_output_v1_add_listener(xdg_output, &xdg_output_listener, &state);

    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (!state.done) {
        fprintf(stderr, "xdg-output: no done event received\n");
        failed = 1;
    } else if (!state.has_position || state.x != 0 || state.y != 0) {
        fprintf(stderr, "xdg-output: expected logical_position 0,0 got %d,%d\n",
                state.x, state.y);
        failed = 1;
    } else if (!state.has_size || state.width == 0 || state.height == 0) {
        fprintf(stderr,
                "xdg-output: expected non-zero logical_size got %dx%d\n",
                state.width, state.height);
        failed = 1;
    }

    zxdg_output_v1_destroy(xdg_output);
    zxdg_output_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return failed;
}
