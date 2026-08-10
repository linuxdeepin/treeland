// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/ddm/ddminterfacev1.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
DDMInterfaceV1 *g_ddm = nullptr;
}

void protocol_test_setup(WServer *server)
{
    // The treeland_ddm_v1 global needs no other globals (wl_compositor and
    // friends are already provided by Treeland::initTestServer). Its request
    // handlers call Helper::instance(), but Helper only exists in the full
    // treeland QML boot, so the client tests only observe the module's own
    // public state (isConnected()) instead of sending requests.
    g_ddm = server->attach<DDMInterfaceV1>();
}

extern "C" void ddm_check_is_connected(void *data)
{
    if (!data)
        return;
    *static_cast<int *>(data) = (g_ddm && g_ddm->isConnected()) ? 1 : 0;
}
