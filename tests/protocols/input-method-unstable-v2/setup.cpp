// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "server-bridge.h"
#include "server-bridge-api.h"
#include "core/waylandserver.h"
#include "rootsurfacecontainer.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wseat.h>

namespace {
struct ActivateToplevelData {
    int index = 0;
    bool activated = false;
};

void activateToplevel(void *data)
{
    auto *request = static_cast<ActivateToplevelData *>(data);
    auto *server = WaylandServer::instance();
    auto *seat = server ? server->seat() : nullptr;
    if (!server || !seat)
        return;

    int index = 0;
    const auto surfaces = server->rootSurfaceContainer()->surfaces();
    for (auto *wrapper : surfaces) {
        if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
            continue;
        if (index++ != request->index)
            continue;
        server->activateSurface(wrapper, Qt::ActiveWindowFocusReason, seat);
        request->activated = true;
        return;
    }
}
}

extern "C" int protocol_test_activate_toplevel(int index)
{
    ActivateToplevelData data;
    data.index = index;
    if (!invoke_on_server_thread(activateToplevel, &data))
        return 0;
    return data.activated ? 1 : 0;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
}
