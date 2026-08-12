// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"

#include <wseat.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WindowOverlapCheckerInterface *g_checker = nullptr;
DDEActiveInterface *g_active = nullptr;
WindowPickerInterface *g_picker = nullptr;
}

void protocol_test_setup(WServer *server)
{
    server->attach<WSeat>();
    auto *manager = server->attach<DDEShellManagerInterfaceV1>();
    QObject::connect(manager, &DDEShellManagerInterfaceV1::windowOverlapCheckerCreated,
                     [](WindowOverlapCheckerInterface *checker) { g_checker = checker; });
    QObject::connect(manager, &DDEShellManagerInterfaceV1::activeCreated,
                     [](DDEActiveInterface *active) { g_active = active; });
    QObject::connect(manager, &DDEShellManagerInterfaceV1::PickerCreated,
                     [](WindowPickerInterface *picker) { g_picker = picker; });
}

extern "C" void dde_shell_emit_test_events(void *)
{
    if (g_checker) {
        g_checker->sendOverlapped(true);
        g_checker->sendOverlapped(false);
    }
    if (g_active) {
        DDEActiveInterface::sendActiveIn(0, g_active->seat());
        DDEActiveInterface::sendActiveOut(1, g_active->seat());
        DDEActiveInterface::sendStartDrag(g_active->seat());
        DDEActiveInterface::sendDrop(g_active->seat());
    }
    if (g_picker)
        g_picker->sendWindowPid(42);
}
