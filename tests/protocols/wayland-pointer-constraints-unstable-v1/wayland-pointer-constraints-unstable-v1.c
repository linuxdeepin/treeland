// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zwp_pointer_constraints_v1 global served by Treeland
// (WPointerConstraintsV1 in Helper).  The client obtains a wl_pointer from the
// pointer-capable seat, creates a wl_surface, and requests a locked pointer
// constraint — verifying the constraint object is created successfully.

#include "client-connection.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"

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
        fprintf(stderr, "pointer-constraints: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "pointer-constraints: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }
    g_caps = 0;
    wl_seat_add_listener(seat, &seat_listener, NULL);
    wl_display_roundtrip(conn.display);

    if (!(g_caps & 0x1 /* WL_SEAT_CAPABILITY_POINTER */)) {
        fprintf(stderr, "pointer-constraints: seat has no pointer capability\n");
        client_disconnect(&conn);
        return 1;
    }
    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "pointer-constraints: wl_seat_get_pointer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    if (!compositor) {
        fprintf(stderr, "pointer-constraints: no wl_compositor global\n");
        client_disconnect(&conn);
        return 1;
    }
    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "pointer-constraints: create_surface returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }
    wl_surface_commit(surface);

    struct zwp_pointer_constraints_v1 *manager = client_bind(
        &conn, "zwp_pointer_constraints_v1",
        &zwp_pointer_constraints_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "pointer-constraints: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_locked_pointer_v1 *locked =
        zwp_pointer_constraints_v1_lock_pointer(
            manager, surface, pointer, NULL,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
    if (!locked) {
        fprintf(stderr, "pointer-constraints: lock_pointer returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }

    wl_display_roundtrip(conn.display);

    zwp_locked_pointer_v1_destroy(locked);
    wl_surface_destroy(surface);
    zwp_pointer_constraints_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
