// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "pointer-gestures-unstable-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    if (!seat) {
        fprintf(stderr, "pointer-gestures: failed to bind seat\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "pointer-gestures: seat has no pointer capability\n");
        client_disconnect(&conn);
        return 1;
    }
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "pointer-gestures: roundtrip after get_pointer failed\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_pointer_gestures_v1 *manager =
        client_bind(&conn, "zwp_pointer_gestures_v1",
                    &zwp_pointer_gestures_v1_interface, 3);
    if (!manager) {
        fprintf(stderr, "pointer-gestures: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Create all three gesture types (swipe, pinch, hold). */
    struct zwp_pointer_gesture_swipe_v1 *swipe =
        zwp_pointer_gestures_v1_get_swipe_gesture(manager, pointer);
    struct zwp_pointer_gesture_pinch_v1 *pinch =
        zwp_pointer_gestures_v1_get_pinch_gesture(manager, pointer);
    struct zwp_pointer_gesture_hold_v1 *hold =
        zwp_pointer_gestures_v1_get_hold_gesture(manager, pointer);
    if (!swipe || !pinch || !hold) {
        fprintf(stderr, "pointer-gestures: failed to create gesture objects\n");
        client_disconnect(&conn);
        return 1;
    }

    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "pointer-gestures: roundtrip failed\n");
        return 1;
    }

    /* v3 adds release() as destructor for the manager. */
    zwp_pointer_gestures_v1_release(manager);
    wl_pointer_destroy(pointer);
    client_disconnect(&conn);
    return 0;
}
