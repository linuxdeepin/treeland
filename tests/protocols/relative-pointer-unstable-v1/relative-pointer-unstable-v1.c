// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "relative-pointer-unstable-v1-client-protocol.h"

#include <stdio.h>

static void handle_relative_motion(void *data,
                                   struct zwp_relative_pointer_v1 *rp,
                                   uint32_t utime_hi, uint32_t utime_lo,
                                   wl_fixed_t dx, wl_fixed_t dy,
                                   wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
    (void)data; (void)rp; (void)utime_hi; (void)utime_lo;
    (void)dx; (void)dy; (void)dx_unaccel; (void)dy_unaccel;
}

static const struct zwp_relative_pointer_v1_listener listener = {
    .relative_motion = handle_relative_motion,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    if (!seat) {
        fprintf(stderr, "relative-pointer: failed to bind seat\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "relative-pointer: no pointer capability\n");
        client_disconnect(&conn);
        return 1;
    }
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "relative-pointer: roundtrip failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_relative_pointer_manager_v1 *mgr =
        client_bind(&conn, "zwp_relative_pointer_manager_v1",
                    &zwp_relative_pointer_manager_v1_interface, 1);
    if (!mgr) {
        fprintf(stderr, "relative-pointer: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_relative_pointer_v1 *rp =
        zwp_relative_pointer_manager_v1_get_relative_pointer(mgr, pointer);
    if (!rp) {
        fprintf(stderr, "relative-pointer: get_relative_pointer null\n");
        client_disconnect(&conn);
        return 1;
    }

    zwp_relative_pointer_v1_add_listener(rp, &listener, NULL);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "relative-pointer: roundtrip failed\n");
        return 1;
    }

    zwp_relative_pointer_v1_destroy(rp);
    zwp_relative_pointer_manager_v1_destroy(mgr);
    wl_pointer_destroy(pointer);
    client_disconnect(&conn);
    return 0;
}
