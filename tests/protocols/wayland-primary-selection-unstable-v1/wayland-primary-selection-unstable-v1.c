// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zwp_primary_selection_device_manager_v1 global served by Treeland
// (wlr_primary_selection_v1_device_manager_create in Helper).  The client
// creates a primary-selection device from the seat and verifies the device
// object is created successfully.  (In headless mode without keyboard focus
// wlroots does not emit an initial selection event, so we test device
// creation only.)

#include "client-connection.h"
#include "primary-selection-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "primary-selection: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "primary-selection: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_primary_selection_device_manager_v1 *manager = client_bind(
        &conn, "zwp_primary_selection_device_manager_v1",
        &zwp_primary_selection_device_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "primary-selection: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_primary_selection_device_v1 *device =
        zwp_primary_selection_device_manager_v1_get_device(manager, seat);
    if (!device) {
        fprintf(stderr, "primary-selection: get_device returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    wl_display_roundtrip(conn.display);

    zwp_primary_selection_device_v1_destroy(device);
    zwp_primary_selection_device_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
