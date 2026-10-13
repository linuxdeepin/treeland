// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "treeland-decoration-unstable-v1.h"

#include "core/shellhandler.h"
#include "modules/decoration/decorationmanagerinterfacev1.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_toplevel = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    Q_ASSERT(find_server_interface<DecorationManagerInterfaceV1>(server));
    add_headless_output(server->backend(), false);
    QObject::connect(server->shellHandler(), &ShellHandler::surfaceWrapperAdded, server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_toplevel = wrapper;
                     });
}

extern "C" void decoration_read_server_state(void *data)
{
    auto *state = static_cast<decoration_server_state *>(data);
    *state = {};
    if (!g_toplevel)
        return;

    state->valid = 1;
    state->radius = g_toplevel->radius();
}
