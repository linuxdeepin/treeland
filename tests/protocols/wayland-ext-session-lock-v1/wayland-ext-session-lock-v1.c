/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the ext_session_lock_manager_v1 global served by Treeland's
 * WSessionLockManager.
 *
 * Coverage level E (end-to-end): the client locks the session and, upon
 * receiving the `locked` event, reads back the production WSessionLock state
 * over the server bridge.  WSessionLock::isLocked() must return true, proving
 * the lock reached the real compositor session-lock object rather than merely
 * surviving without a protocol error.  The compositor may also deny the lock
 * immediately by sending `finished` (e.g. when the lock screen is already
 * visible); in that case the test reports failure, since unconditional lock
 * rejection would be a regression.
 */

#include "wayland-ext-session-lock-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "ext-session-lock-v1-client-protocol.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

struct lock_state {
	int locked;
	int finished;
};

static void locked(void *data, struct ext_session_lock_v1 *lock) {
	(void)lock;
	((struct lock_state *)data)->locked = 1;
}

static void finished(void *data, struct ext_session_lock_v1 *lock) {
	(void)lock;
	((struct lock_state *)data)->finished = 1;
}

static const struct ext_session_lock_v1_listener lock_listener = {
	.locked = locked,
	.finished = finished,
};

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("ext-session-lock: connect failed\n");
		return 1;
	}

	struct ext_session_lock_manager_v1 *manager = client_bind(
		&conn, ext_session_lock_manager_v1_interface.name, &ext_session_lock_manager_v1_interface, 1);
	if (manager == NULL) {
		TEST_ERROR("ext-session-lock: failed to bind manager: %s\n", strerror(errno));
		client_disconnect(&conn);
		return 1;
	}

	struct lock_state state;
	memset(&state, 0, sizeof(state));
	struct ext_session_lock_v1 *lock = ext_session_lock_manager_v1_lock(manager);
	if (lock == NULL) {
		TEST_ERROR("ext-session-lock: lock returned NULL\n");
		ext_session_lock_manager_v1_destroy(manager);
		client_disconnect(&conn);
		return 1;
	}
	ext_session_lock_v1_add_listener(lock, &lock_listener, &state);

	/* Treeland uses a 300 ms grace timer (Helper::onExtSessionLock) before
	 * acknowledging the lock.  Allow enough iterations to cover the timer. */
	for (int i = 0; i < 20 && !state.locked && !state.finished; i++) {
		wl_display_roundtrip(conn.display);
		if (!state.locked && !state.finished)
			usleep(50000);
	}

	int failed = 0;
	if (!state.locked && !state.finished) {
		TEST_ERROR("ext-session-lock: no locked or finished event received\n");
		ext_session_lock_v1_destroy(lock);
		failed = 1;
	} else if (state.locked) {
		/* E-level: the production WSessionLock must actually be locked. */
		struct session_lock_server_state srv;
		memset(&srv, 0, sizeof(srv));
		if (invoke_on_server_thread(session_lock_read_server_state, &srv) == 0) {
			TEST_ERROR("ext-session-lock: failed to read server state\n");
			failed = 1;
		} else if (!srv.valid) {
			TEST_ERROR("ext-session-lock: no WSessionLock captured\n");
			failed = 1;
		} else if (!srv.locked) {
			TEST_ERROR("ext-session-lock: WSessionLock::isLocked() is false\n");
			failed = 1;
		}

		/* Unlock the session cleanly.  Treeland does not send `finished`
		 * on client-initiated unlock, so we only flush the request. */
		ext_session_lock_v1_unlock_and_destroy(lock);
		wl_display_roundtrip(conn.display);
	} else {
		/* Compositor sent `finished` (lock rejected).  This is a failure:
		 * if the compositor unconditionally rejects lock requests, the
		 * session lock feature is broken.  Destroy the lock proxy
		 * explicitly to send the destroy request. */
		ext_session_lock_v1_destroy(lock);
		failed = 1;
	}

	ext_session_lock_manager_v1_destroy(manager);
	client_disconnect(&conn);
	return failed;
}
