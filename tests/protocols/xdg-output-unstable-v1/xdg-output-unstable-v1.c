// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <stdio.h>

struct output_info {
    int has_position;
    int has_size;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

static void handle_logical_position(void *data, struct zxdg_output_v1 *output,
                                    int32_t x, int32_t y)
{
    (void)output;
    struct output_info *info = data;
    info->has_position = 1;
    info->x = x;
    info->y = y;
}

static void handle_logical_size(void *data, struct zxdg_output_v1 *output,
                                int32_t width, int32_t height)
{
    (void)output;
    struct output_info *info = data;
    info->has_size = 1;
    info->width = width;
    info->height = height;
}

static void handle_done(void *data, struct zxdg_output_v1 *output)
{
    (void)data;
    (void)output;
}

static void handle_name(void *data, struct zxdg_output_v1 *output, const char *name)
{
    (void)data;
    (void)output;
    (void)name;
}

static void handle_description(void *data, struct zxdg_output_v1 *output,
                               const char *description)
{
    (void)data;
    (void)output;
    (void)description;
}

static const struct zxdg_output_v1_listener listener = {
    .logical_position = handle_logical_position,
    .logical_size = handle_logical_size,
    .done = handle_done,
    .name = handle_name,
    .description = handle_description,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_output *output =
        client_bind(&conn, "wl_output", &wl_output_interface, 3);
    struct zxdg_output_manager_v1 *manager =
        client_bind(&conn, "zxdg_output_manager_v1",
                    &zxdg_output_manager_v1_interface, 3);
    if (!output || !manager) {
        fprintf(stderr, "xdg-output: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct output_info info = {0};
    struct zxdg_output_v1 *xdg_output =
        zxdg_output_manager_v1_get_xdg_output(manager, output);
    if (!xdg_output) {
        fprintf(stderr, "xdg-output: get_xdg_output returned null\n");
        goto fail;
    }
    zxdg_output_v1_add_listener(xdg_output, &listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-output: roundtrip after get_xdg_output failed\n");
        goto fail;
    }

    /* The compositor must send logical geometry for the headless output. */
    if (!info.has_position || !info.has_size) {
        fprintf(stderr, "xdg-output: missing logical_position/logical_size events\n");
        goto fail;
    }
    if (info.x != 0 || info.y != 0) {
        fprintf(stderr, "xdg-output: expected logical position (0,0), got (%d,%d)\n",
                info.x, info.y);
        goto fail;
    }
    if (info.width <= 0 || info.height <= 0) {
        fprintf(stderr, "xdg-output: invalid logical size %dx%d\n",
                info.width, info.height);
        goto fail;
    }

    zxdg_output_v1_destroy(xdg_output);
    zxdg_output_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;

fail:
    client_disconnect(&conn);
    return 1;
}
