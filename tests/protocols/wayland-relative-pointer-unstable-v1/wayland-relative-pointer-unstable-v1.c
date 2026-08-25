// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zwp_relative_pointer_manager_v1 global served by Treeland
// (wlr_relative_pointer_manager_v1_create).  The client obtains a wl_pointer
// from the (pointer-capable) seat and creates a relative-pointer object.

#include "client-connection.h"
#include "relative-pointer-unstable-v1-client-protocol.h"

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
        fprintf(stderr, "relative-pointer: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "relative-pointer: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }
    g_caps = 0;
    wl_seat_add_listener(seat, &seat_listener, NULL);
    wl_display_roundtrip(conn.display);

    if (!(g_caps & 0x1)) {
        fprintf(stderr, "relative-pointer: seat has no pointer capability\n");
        client_disconnect(&conn);
        return 1;
    }
    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "relative-pointer: wl_seat_get_pointer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_relative_pointer_manager_v1 *manager = client_bind(
        &conn, "zwp_relative_pointer_manager_v1",
        &zwp_relative_pointer_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "relative-pointer: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_relative_pointer_v1 *rel =
        zwp_relative_pointer_manager_v1_get_relative_pointer(manager, pointer);
    if (!rel) {
        fprintf(stderr, "relative-pointer: get_relative_pointer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    wl_display_roundtrip(conn.display);

    zwp_relative_pointer_v1_destroy(rel);
    zwp_relative_pointer_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
