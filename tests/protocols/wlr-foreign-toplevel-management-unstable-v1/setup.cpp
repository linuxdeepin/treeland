// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "workspace/workspace.h"

#include <woutputrenderwindow.h>
#include <wbackend.h>
#include <wforeigntoplevelv1.h>

#include "treelandconfig.hpp"

namespace {
SurfaceWrapper *g_wrapper = nullptr;
int g_maximizeRequestCount = 0;
int g_lastMaximizeRequest = -1;
int g_maximizedAfterLastRequest = -1;
}

void protocol_test_setup(WaylandServer *server)
{
    // This test asserts the foreign-toplevel protocol's state transitions, not
    // the geometry animation. With the animation off every maximize /
    // fullscreen / restore request applies its surface state directly, so the
    // test neither paces frames nor waits for an animation completion signal
    // (same approach as the v3 foreign-toplevel test).
    // Medium disables animations while preserving window effects.
    if (auto *globalConfig = server->globalConfig())
        globalConfig->setEffectLevel(QStringLiteral("medium"));
    add_headless_output(server->backend(), false);
    auto *foreignToplevel = server->backend()->server()->findInterface<WForeignToplevel>();
    Q_ASSERT(foreignToplevel);
    QObject::connect(foreignToplevel,
                     &WForeignToplevel::requestMaximize,
                     server,
                     [](WToplevelSurface *, bool maximized) {
                         ++g_maximizeRequestCount;
                         g_lastMaximizeRequest = maximized;
                         g_maximizedAfterLastRequest = g_wrapper && g_wrapper->isMaximized();
                     });
    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

extern "C" void foreign_toplevel_read_server_state(void *data)
{
    foreign_toplevel_server_state state {};
    auto *server = WaylandServer::instance();
    state.output_ready = !server->rootSurfaceContainer()->outputs().isEmpty();
    state.wrapper_created = g_wrapper != nullptr;
    state.wrapper_in_workspace = g_wrapper && server->workspace()->surfaces().contains(g_wrapper);
    state.minimized = g_wrapper && g_wrapper->shellSurface() && g_wrapper->shellSurface()->isMinimized();
    state.maximized = g_wrapper && g_wrapper->isMaximized();
    state.fullscreen = g_wrapper && g_wrapper->surfaceState() == SurfaceWrapper::State::Fullscreen;
    state.activated = g_wrapper && g_wrapper->isActivated();
    state.maximize_request_count = g_maximizeRequestCount;
    state.last_maximize_request = g_lastMaximizeRequest;
    state.maximized_after_last_request = g_maximizedAfterLastRequest;
    const auto *seatContainer = server->rootSurfaceContainer()->getSeatContainerOrDefault();
    state.focused = seatContainer && seatContainer->keyboardFocusSurface() == g_wrapper;
    *static_cast<foreign_toplevel_server_state *>(data) = state;
}

extern "C" void foreign_toplevel_render(void *)
{
    if (!g_wrapper)
        return;

    // Geometry animations are disabled in protocol_test_setup(), so the state
    // change has already landed when the request was handled; one render lets a
    // queued scene-graph frame apply it before the test reads it back.
    WaylandServer::instance()->window()->render();
}
