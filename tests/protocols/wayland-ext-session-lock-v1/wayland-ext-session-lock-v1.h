/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_EXT_SESSION_LOCK_TEST_H
#define WAYLAND_EXT_SESSION_LOCK_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real WSessionLock state, read on the compositor (Qt) thread.
 * Treeland's Helper::onExtSessionLock starts a 300 ms grace timer and then
 * calls WSessionLock::lock(), which sends the `locked` event to the client and
 * flips the production WSessionLock to the Locked state.  An E-level test
 * verifies that WSessionLock::isLocked() returns true after the client receives
 * the `locked` event, proving the lock reached the real compositor session-lock
 * object rather than merely surviving without a protocol error.
 */
struct session_lock_server_state {
	int valid; /* a WSessionLock was captured from lockCreated */
	int locked; /* WSessionLock::isLocked() */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the lock state. */
void session_lock_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_EXT_SESSION_LOCK_TEST_H */
