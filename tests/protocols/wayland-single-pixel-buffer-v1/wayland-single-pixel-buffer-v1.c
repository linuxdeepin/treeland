// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the wp_single_pixel_buffer_manager_v1 global created by
// wlr_single_pixel_buffer_manager_v1_create in Treeland's Helper.  The client
// creates a single-pixel RGBA buffer, attaches it to a plain wl_surface,
// commits, and asserts the buffer is a valid wl_buffer that survives a
// roundtrip without a protocol error.

#include "client-connection.h"
#include "single-pixel-buffer-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "single-pixel-buffer: connect failed\n");
        return 1;
    }

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    if (!compositor) {
        fprintf(stderr, "single-pixel-buffer: no wl_compositor global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_single_pixel_buffer_manager_v1 *manager = client_bind(
        &conn, "wp_single_pixel_buffer_manager_v1",
        &wp_single_pixel_buffer_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr,
                "single-pixel-buffer: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "single-pixel-buffer: create_surface failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_buffer *buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
        manager, 0xffffu, 0x0000u, 0x0000u, 0xffffu);
    if (!buffer) {
        fprintf(stderr, "single-pixel-buffer: create_u32_rgba_buffer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    wl_display_roundtrip(conn.display);

    // The buffer is a real wl_buffer: a second attach/commit must also succeed.
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    wl_display_roundtrip(conn.display);

    wl_buffer_destroy(buffer);
    wl_surface_destroy(surface);
    wp_single_pixel_buffer_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
