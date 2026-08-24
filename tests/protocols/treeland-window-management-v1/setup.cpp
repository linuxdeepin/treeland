// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/window-management/windowmanagementinterfacev1.h"
#include "server-bridge.h"

#include <wserver.h>

#include <stdint.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WindowManagementInterfaceV1 *g_windowManagement = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    g_windowManagement = find_server_interface<WindowManagementInterfaceV1>(helper);
}

extern "C" void window_management_get_desktop_state(void *data)
{
    uint32_t *out = static_cast<uint32_t *>(data);
    *out = g_windowManagement
               ? static_cast<uint32_t>(g_windowManagement->desktopState())
               : UINT32_MAX;
}

extern "C" void window_management_set_desktop_state(void *data)
{
    if (!g_windowManagement)
        return;
    const uint32_t state = *static_cast<const uint32_t *>(data);
    g_windowManagement->setDesktopState(static_cast<WindowManagementInterfaceV1::DesktopState>(state));
}
