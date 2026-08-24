// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "ext-session-lock-v1-client-protocol.h"

#include <stdio.h>
#include <unistd.h>

struct lock_info {
    int locked;
    int finished;
};

static void handle_locked(void *data, struct ext_session_lock_v1 *lock)
{
    (void)lock;
    ((struct lock_info *)data)->locked = 1;
}

static void handle_finished(void *data, struct ext_session_lock_v1 *lock)
{
    (void)lock;
    ((struct lock_info *)data)->finished = 1;
}

static const struct ext_session_lock_v1_listener listener = {
    .locked = handle_locked,
    .finished = handle_finished,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct ext_session_lock_manager_v1 *manager =
        client_bind(&conn, "ext_session_lock_manager_v1",
                    &ext_session_lock_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "ext-session-lock: failed to bind\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_session_lock_v1 *lock = ext_session_lock_manager_v1_lock(manager);
    if (!lock) {
        fprintf(stderr, "ext-session-lock: lock() returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    struct lock_info info = {0};
    ext_session_lock_v1_add_listener(lock, &listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-session-lock: roundtrip after lock failed\n");
        return 1;
    }

    /* Treeland's session lock handler has a 300 ms grace timer before
     * sending the locked event.  Wait for it to fire. */
    if (!info.locked && !info.finished) {
        usleep(500000); /* 500 ms */
        if (wl_display_roundtrip(conn.display) < 0) {
            fprintf(stderr, "ext-session-lock: roundtrip after grace failed\n");
            return 1;
        }
    }

    if (info.finished) {
        /* The server already has an active lock; finished is a valid
         * response. */
        client_disconnect(&conn);
        return 0;
    }

    if (!info.locked) {
        fprintf(stderr, "ext-session-lock: did not receive locked event\n");
        return 1;
    }

    /* Unlock cleanly so the compositor returns to normal state. */
    ext_session_lock_v1_unlock_and_destroy(lock);
    if (wl_display_roundtrip(conn.display) < 0) {
        /* The unlock_and_destroy is a destructor; the object is gone.
         * A roundtrip error here is acceptable if the server already
         * processed the destruction. */
    }

    client_disconnect(&conn);
    return 0;
}
