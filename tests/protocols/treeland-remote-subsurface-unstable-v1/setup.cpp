// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "treeland-remote-subsurface-unstable-v1.h"

#include "core/shellhandler.h"
#include "protocols/wremotesubsurfacemanagerv1.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wsubsurface.h>
#include <wsurface.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
SurfaceWrapper *g_parent = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    Q_ASSERT(find_server_interface<WRemoteSubsurfaceManagerV1>(server));
    add_headless_output(server->backend(), false);
    QObject::connect(server->shellHandler(), &ShellHandler::surfaceWrapperAdded, server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_parent = wrapper;
                     });
}

extern "C" void remote_subsurface_read_server_state(void *data)
{
    auto *state = static_cast<remote_subsurface_server_state *>(data);
    *state = {};
    if (!g_parent || !g_parent->surface())
        return;

    const auto &subsurfaces = g_parent->surface()->subsurfaces();
    if (subsurfaces.isEmpty())
        return;

    auto *subsurface = subsurfaces.constFirst();
    if (!subsurface)
        return;

    state->valid = 1;
    state->parent_matches = subsurface->parentSurface() == g_parent->surface();
    state->type = static_cast<int>(subsurface->type());
    state->place = static_cast<int>(subsurface->place());
    const auto position = subsurface->position();
    state->x = position.x();
    state->y = position.y();
}
