// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/ddm/ddminterfacev1.h"
#include "server-bridge.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
DDMInterfaceV1 *g_ddm = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    g_ddm = find_server_interface<DDMInterfaceV1>(helper);
}

extern "C" void ddm_check_is_connected(void *data)
{
    if (!data)
        return;
    *static_cast<int *>(data) = (g_ddm && g_ddm->isConnected()) ? 1 : 0;
}
