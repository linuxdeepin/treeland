// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the wp_cursor_shape_manager_v1 global served by Treeland
// (wlr_cursor_shape_manager_v1_create).  The client obtains a wl_pointer from
// the (pointer-capable) seat and creates a cursor-shape device for it.

#include "client-connection.h"
#include "cursor-shape-v1-client-protocol.h"

#include <stdio.h>

static uint32_t g_caps;
static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps)
{
    (void)data; (void)seat;
    g_caps = caps;
}
static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data; (void)seat; (void)name;
}
static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_caps,
    .name = seat_name,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "cursor-shape: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "cursor-shape: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }
    g_caps = 0;
    wl_seat_add_listener(seat, &seat_listener, NULL);
    wl_display_roundtrip(conn.display);

    if (!(g_caps & 0x1 /* WL_SEAT_CAPABILITY_POINTER */)) {
        fprintf(stderr, "cursor-shape: seat has no pointer capability\n");
        client_disconnect(&conn);
        return 1;
    }
    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "cursor-shape: wl_seat_get_pointer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_cursor_shape_manager_v1 *manager = client_bind(
        &conn, "wp_cursor_shape_manager_v1",
        &wp_cursor_shape_manager_v1_interface, 2);
    if (!manager) {
        fprintf(stderr, "cursor-shape: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_cursor_shape_device_v1 *device =
        wp_cursor_shape_manager_v1_get_pointer(manager, pointer);
    if (!device) {
        fprintf(stderr, "cursor-shape: get_pointer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    wp_cursor_shape_device_v1_set_shape(device, 0, 1 /* DEFAULT */);
    wl_display_roundtrip(conn.display);

    wp_cursor_shape_device_v1_destroy(device);
    wp_cursor_shape_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
