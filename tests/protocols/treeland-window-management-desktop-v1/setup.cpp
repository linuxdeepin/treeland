// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-window-management-desktop-v1.h"
#include "workspace/workspace.h"

#include <QEventLoop>
#include <QTimer>

#include <wbackend.h>
#include <woutputrenderwindow.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
window_management_desktop_state g_state {};
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void window_management_desktop_read_state(void *data)
{
    auto state = g_state;
    state.desktop_state = static_cast<unsigned int>(Helper::instance()->showDesktopState());
    if (g_wrapper) {
        const auto paintOrder = WOutputRenderWindow::paintOrderItemList(
            Helper::instance()->workspace(),
            [](QQuickItem *) { return true; });
        state.wrapper_in_paint_order = paintOrder.contains(g_wrapper) ? 1 : 0;
        state.wrapper_visible = g_wrapper->isVisible() ? 1 : 0;
        state.wrapper_minimized = g_wrapper->isMinimized() ? 1 : 0;
    }
    *static_cast<window_management_desktop_state *>(data) = state;
}

extern "C" void window_management_desktop_wait_visible(void *data)
{
    auto *wait = static_cast<window_management_desktop_visibility_wait *>(data);
    QEventLoop loop;
    QTimer poll;
    QTimer timeout;
    poll.setInterval(10);
    timeout.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (g_wrapper && g_wrapper->isVisible() == bool(wait->visible)) {
            wait->reached = 1;
            loop.quit();
        }
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    timeout.start(2000);
    loop.exec();
}
