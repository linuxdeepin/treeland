// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "cursor-shape-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    if (!seat) {
        fprintf(stderr, "cursor-shape: failed to bind seat\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Need pointer capability — provided by the test pointer device in setup. */
    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "cursor-shape: seat has no pointer capability\n");
        client_disconnect(&conn);
        return 1;
    }
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "cursor-shape: roundtrip after get_pointer failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_cursor_shape_manager_v1 *manager =
        client_bind(&conn, "wp_cursor_shape_manager_v1",
                    &wp_cursor_shape_manager_v1_interface, 2);
    if (!manager) {
        fprintf(stderr, "cursor-shape: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wp_cursor_shape_device_v1 *device =
        wp_cursor_shape_manager_v1_get_pointer(manager, pointer);
    if (!device) {
        fprintf(stderr, "cursor-shape: get_pointer returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Set cursor to default shape (enum value 1). */
    wp_cursor_shape_device_v1_set_shape(device, 0,
                                        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "cursor-shape: roundtrip after set_shape failed\n");
        return 1;
    }

    wp_cursor_shape_device_v1_destroy(device);
    wp_cursor_shape_manager_v1_destroy(manager);
    wl_pointer_destroy(pointer);
    client_disconnect(&conn);
    return 0;
}
