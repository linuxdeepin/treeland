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

#include <QEventLoop>
#include <QQuickItem>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
int g_maximizeRequestCount = 0;
int g_lastMaximizeRequest = -1;
int g_maximizedAfterLastRequest = -1;
int g_animationRunningAfterLastRequest = -1;
}

void protocol_test_setup(WaylandServer *server)
{
    // Keep production geometry transitions short, but wait for their real
    // completion signal below instead of sampling a timing-dependent frame.
    server->setAnimationSpeed(0.1f);
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
                         g_animationRunningAfterLastRequest = g_wrapper && g_wrapper->isAnimationRunning();
                     });
    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [server](SurfaceWrapper *wrapper) {
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
    state.animation_running_after_last_request = g_animationRunningAfterLastRequest;
    state.animation_running = g_wrapper && g_wrapper->isAnimationRunning();
    const auto *seatContainer = server->rootSurfaceContainer()->getSeatContainerOrDefault();
    state.focused = seatContainer && seatContainer->keyboardFocusSurface() == g_wrapper;
    *static_cast<foreign_toplevel_server_state *>(data) = state;
}

extern "C" void foreign_toplevel_render(void *)
{
    if (!g_wrapper)
        return;
    auto *server = WaylandServer::instance();
    server->window()->render();
    if (!g_wrapper->isAnimationRunning())
        return;

    QQuickItem *geometryAnimation = nullptr;
    for (auto *item : g_wrapper->container()->childItems()) {
        if (item->property("surface").value<SurfaceWrapper *>() == g_wrapper) {
            geometryAnimation = item;
            break;
        }
    }
    if (!geometryAnimation)
        return;
    QEventLoop eventLoop;
    QObject::connect(geometryAnimation, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();
}
