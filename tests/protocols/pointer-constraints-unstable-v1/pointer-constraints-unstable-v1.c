// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"

#include <stdio.h>

static void handle_locked(void *data, struct zwp_locked_pointer_v1 *lp)
{
    (void)data; (void)lp;
}

static void handle_unlocked(void *data, struct zwp_locked_pointer_v1 *lp)
{
    (void)data; (void)lp;
}

static const struct zwp_locked_pointer_v1_listener listener = {
    .locked = handle_locked,
    .unlocked = handle_unlocked,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "pointer-constraints: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    if (!seat) {
        fprintf(stderr, "pointer-constraints: failed to bind seat\n");
        goto fail;
    }

    struct wl_pointer *pointer = wl_seat_get_pointer(seat);
    if (!pointer) {
        fprintf(stderr, "pointer-constraints: no pointer capability\n");
        goto fail;
    }
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "pointer-constraints: roundtrip failed\n");
        goto fail;
    }

    struct zwp_pointer_constraints_v1 *mgr =
        client_bind(&conn, "zwp_pointer_constraints_v1",
                    &zwp_pointer_constraints_v1_interface, 1);
    if (!mgr) {
        fprintf(stderr, "pointer-constraints: failed to bind manager\n");
        goto fail;
    }

    /* Lock pointer on the surface with persistent lifetime.
     * region=NULL means the entire surface. Without pointer focus the
     * locked event won't arrive, but object creation must succeed. */
    struct zwp_locked_pointer_v1 *lp =
        zwp_pointer_constraints_v1_lock_pointer(mgr, tc.surface, pointer,
                                                 NULL,
                                                 ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    if (!lp) {
        fprintf(stderr, "pointer-constraints: lock_pointer returned null\n");
        goto fail;
    }

    zwp_locked_pointer_v1_add_listener(lp, &listener, NULL);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "pointer-constraints: roundtrip after lock failed\n");
        return 1;
    }

    zwp_locked_pointer_v1_destroy(lp);
    zwp_pointer_constraints_v1_destroy(mgr);
    wl_pointer_destroy(pointer);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;

fail:
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
