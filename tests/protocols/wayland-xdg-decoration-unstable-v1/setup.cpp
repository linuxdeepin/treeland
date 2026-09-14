// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-decoration-unstable-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <protocols/wxdgdecorationmanager.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
SurfaceWrapper *g_wrapper = nullptr;
WXdgDecorationManager *g_decoMgr = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_decoMgr = find_server_interface<WXdgDecorationManager>(helper);

    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

void xdg_decoration_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_decoration_server_state *>(data);
    state->valid = 0;
    state->mode = 0;

    if (!g_wrapper)
        return;

    auto *wsurface = g_wrapper->surface();
    if (!wsurface)
        return;

    state->valid = 1;

    if (g_decoMgr) {
        state->mode = static_cast<int>(g_decoMgr->modeBySurface(wsurface));
    }
}
