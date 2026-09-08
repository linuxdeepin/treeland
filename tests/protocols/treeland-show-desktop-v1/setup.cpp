// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/show-desktop/showdesktopinterfacev1.h"
#include "server-bridge.h"

#include <wserver.h>

#include <stdint.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
ShowDesktopInterfaceV1 *g_showDesktop = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    g_showDesktop = find_server_interface<ShowDesktopInterfaceV1>(helper);
}

extern "C" void show_desktop_get_state(void *data)
{
    uint32_t *out = static_cast<uint32_t *>(data);
    *out = g_showDesktop
               ? static_cast<uint32_t>(g_showDesktop->desktopState())
               : UINT32_MAX;
}

extern "C" void show_desktop_set_state(void *data)
{
    if (!g_showDesktop)
        return;
    const uint32_t state = *static_cast<const uint32_t *>(data);
    g_showDesktop->setDesktopState(static_cast<ShowDesktopInterfaceV1::State>(state));
}
