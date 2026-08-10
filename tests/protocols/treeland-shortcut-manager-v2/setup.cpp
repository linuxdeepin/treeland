// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Server-side fixture for the treeland-shortcut-manager-v2 protocol test.

#include "modules/shortcut/shortcutmanager.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(WServer *server)
{
    // ShortcutManagerV2 publishes the treeland_shortcut_manager_v2 global.
    // acquire/bind_key/commit take no object arguments and capture_next_shortcut
    // accepts a nullable wl_seat, so no extra globals are required; wl_compositor
    // is already provided by Treeland::initTestServer.
    server->attach<ShortcutManagerV2>();
}
