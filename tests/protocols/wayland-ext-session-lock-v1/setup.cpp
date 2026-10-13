// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-ext-session-lock-v1.h"
#include "server-bridge.h"
#include "core/waylandserver.h"

#include <wbackend.h>
#include <protocols/wsessionlockmanager.h>
#include <protocols/wsessionlock.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
// The WSessionLockManager resolved from the server interface list.
WSessionLockManager *g_manager = nullptr;
// The WSessionLock captured from the manager's lockCreated signal.
WSessionLock *g_lock = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    g_manager = find_server_interface<WSessionLockManager>(server);
    if (g_manager) {
        QObject::connect(g_manager, &WSessionLockManager::lockCreated,
                         server, [](WSessionLock *lock) {
                             g_lock = lock;
                         });
    }
}

// E-level read: report the captured production WSessionLock's locked state.
// After the 300 ms grace timer, WaylandServer calls WSessionLock::lock() which sends
// the `locked` Wayland event and sets the internal state to Locked.
void session_lock_read_server_state(void *data)
{
    auto *state = static_cast<struct session_lock_server_state *>(data);
    state->valid = 0;
    state->locked = 0;

    if (!g_lock)
        return;

    state->valid = 1;
    state->locked = g_lock->isLocked() ? 1 : 0;
}
