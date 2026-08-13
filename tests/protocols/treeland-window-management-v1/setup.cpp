// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/window-management/windowmanagementinterfacev1.h"

#include <wserver.h>

#include <stdint.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WindowManagementInterfaceV1 *g_windowManagement = nullptr;
}

void protocol_test_setup(WServer *server)
{
    g_windowManagement = server->attach<WindowManagementInterfaceV1>();
}

/* extern "C" hooks called from the pure-C client via protocol_test_invoke_server.
 * They run on the compositor's Qt thread. */

/* Writes the server-side DesktopState into *data (uint32_t), or UINT32_MAX if
 * the module is missing. */
extern "C" void window_management_get_desktop_state(void *data)
{
    uint32_t *out = static_cast<uint32_t *>(data);
    *out = g_windowManagement
               ? static_cast<uint32_t>(g_windowManagement->desktopState())
               : UINT32_MAX;
}

/* Applies a desktop state from *data (uint32_t) through the module's public
 * API, which also broadcasts the show_desktop event to bound clients.
 * Deliberate server-side stimulus: exercises the event path without a
 * client request. */
extern "C" void window_management_set_desktop_state(void *data)
{
    if (!g_windowManagement)
        return;
    const uint32_t state = *static_cast<const uint32_t *>(data);
    g_windowManagement->setDesktopState(static_cast<WindowManagementInterfaceV1::DesktopState>(state));
}
