// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
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

void protocol_test_setup(Helper *helper)
{
    // Keep production geometry transitions short, but wait for their real
    // completion signal below instead of sampling a timing-dependent frame.
    helper->setAnimationSpeed(0.1f);
    add_headless_output(helper->backend(), false);
    auto *foreignToplevel = helper->backend()->server()->findInterface<WForeignToplevel>();
    Q_ASSERT(foreignToplevel);
    QObject::connect(foreignToplevel,
                     &WForeignToplevel::requestMaximize,
                     helper,
                     [](WToplevelSurface *, bool maximized) {
                         ++g_maximizeRequestCount;
                         g_lastMaximizeRequest = maximized;
                         g_maximizedAfterLastRequest = g_wrapper && g_wrapper->isMaximized();
                         g_animationRunningAfterLastRequest = g_wrapper && g_wrapper->isAnimationRunning();
                     });
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

extern "C" void foreign_toplevel_read_server_state(void *data)
{
    foreign_toplevel_server_state state {};
    auto *helper = Helper::instance();
    state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty();
    state.wrapper_created = g_wrapper != nullptr;
    state.wrapper_in_workspace = g_wrapper && helper->workspace()->surfaces().contains(g_wrapper);
    state.minimized = g_wrapper && g_wrapper->shellSurface() && g_wrapper->shellSurface()->isMinimized();
    state.maximized = g_wrapper && g_wrapper->isMaximized();
    state.fullscreen = g_wrapper && g_wrapper->surfaceState() == SurfaceWrapper::State::Fullscreen;
    state.activated = g_wrapper && g_wrapper->isActivated();
    state.maximize_request_count = g_maximizeRequestCount;
    state.last_maximize_request = g_lastMaximizeRequest;
    state.maximized_after_last_request = g_maximizedAfterLastRequest;
    state.animation_running_after_last_request = g_animationRunningAfterLastRequest;
    state.animation_running = g_wrapper && g_wrapper->isAnimationRunning();
    const auto *seatContainer = helper->rootSurfaceContainer()->getSeatContainerOrDefault();
    state.focused = seatContainer && seatContainer->keyboardFocusSurface() == g_wrapper;
    *static_cast<foreign_toplevel_server_state *>(data) = state;
}

extern "C" void foreign_toplevel_render(void *)
{
    if (!g_wrapper)
        return;
    auto *helper = Helper::instance();
    helper->window()->render();
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
