// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the ext_session_lock_manager_v1 global served by Treeland's
// WSessionLockManager.  The client locks the session and asserts that the
// compositor responds with either `locked` (acknowledged after a 300 ms grace
// timer) or `finished` (denied, e.g. when the lock screen is already visible).
// This exercises the real session-lock lifecycle on the headless output.

#include "client-connection.h"
#include "ext-session-lock-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct lock_state {
    int locked;
    int finished;
};

static void locked(void *data, struct ext_session_lock_v1 *lock)
{
    (void)lock;
    ((struct lock_state *)data)->locked = 1;
}

static void finished(void *data, struct ext_session_lock_v1 *lock)
{
    (void)lock;
    ((struct lock_state *)data)->finished = 1;
}

static const struct ext_session_lock_v1_listener lock_listener = {
    .locked = locked,
    .finished = finished,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "ext-session-lock: connect failed\n");
        return 1;
    }

    struct ext_session_lock_manager_v1 *manager = client_bind(
        &conn, "ext_session_lock_manager_v1",
        &ext_session_lock_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "ext-session-lock: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct lock_state state;
    memset(&state, 0, sizeof(state));
    struct ext_session_lock_v1 *lock = ext_session_lock_manager_v1_lock(manager);
    if (!lock) {
        fprintf(stderr, "ext-session-lock: lock returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }
    ext_session_lock_v1_add_listener(lock, &lock_listener, &state);

    /* Treeland uses a 300 ms grace timer (Helper::onExtSessionLock) before
     * acknowledging the lock.  The compositor may also deny the lock
     * immediately by sending `finished` (e.g. when the lock screen is
     * already visible).  Allow enough iterations to cover the grace timer. */
    for (int i = 0; i < 20 && !state.locked && !state.finished; i++) {
        wl_display_roundtrip(conn.display);
        if (!state.locked && !state.finished)
            usleep(50000);
    }

    int failed = 0;
    if (!state.locked && !state.finished) {
        fprintf(stderr,
                "ext-session-lock: no locked or finished event received\n");
        failed = 1;
    } else if (state.locked) {
        /* Unlock the session cleanly.  Treeland does not send `finished`
         * on client-initiated unlock (it relies on wlr_resource_destroy
         * without calling wlr_session_lock_v1_destroy), so we only flush
         * the request and do not assert on `finished` here. */
        ext_session_lock_v1_unlock_and_destroy(lock);
        wl_display_roundtrip(conn.display);
    }

    ext_session_lock_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return failed;
}
