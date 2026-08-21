// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-foreign-toplevel-manager-v1.h"
#include "workspace/workspace.h"

#include <wbackend.h>
#include <woutputrenderwindow.h>

#include <QEventLoop>
#include <QQuickItem>

#include <cstring>

namespace {
ForeignToplevelManagerInterfaceV1 *g_manager = nullptr;
SurfaceWrapper *g_wrapper = nullptr;
struct ftm_server_state g_state {};
}

void protocol_test_setup(Helper *helper)
{
    // Zero duration races GeometryAnimation::finished with the frame snapshot
    // that emits ready(). A short real animation preserves the production
    // lifecycle while the test waits for its finished signal below.
    helper->setAnimationSpeed(0.1f);
    add_headless_output(helper->backend(), false);
    g_manager = helper->shellHandler()->foreignToplevel();
    QObject::connect(g_manager,
                     &ForeignToplevelManagerInterfaceV1::requestDockPreview,
                     helper,
                     [](auto surfaces, WSurface *, QPoint abs, auto direction) {
                         g_state.preview_fired = 1;
                         g_state.preview_x = abs.x();
                         g_state.preview_y = abs.y();
                         g_state.preview_direction = static_cast<uint32_t>(direction);
                         g_state.preview_surface_count = static_cast<int>(surfaces.size());
                     });
    QObject::connect(g_manager,
                     &ForeignToplevelManagerInterfaceV1::requestDockPreviewTooltip,
                     helper,
                     [](QString tooltip, WSurface *, QPoint abs, auto direction) {
                         g_state.tooltip_fired = 1;
                         const QByteArray utf8 = tooltip.toUtf8();
                         std::strncpy(g_state.tooltip, utf8.constData(), sizeof(g_state.tooltip) - 1);
                         g_state.tooltip[sizeof(g_state.tooltip) - 1] = '\0';
                         g_state.tooltip_x = abs.x();
                         g_state.tooltip_y = abs.y();
                         g_state.tooltip_direction = static_cast<uint32_t>(direction);
                     });
    QObject::connect(g_manager,
                     &ForeignToplevelManagerInterfaceV1::requestDockClose,
                     helper,
                     [] { g_state.close_fired = 1; });
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                         g_state.mapped_xdg_toplevel = wrapper->surface() && wrapper->surface()->mapped() ? 1 : 0;
                     });
}

extern "C" void ftm_read_server_state(void *data)
{
    g_state.output_ready = !Helper::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    g_state.wrapper_minimized = g_wrapper && g_wrapper->shellSurface()
        && g_wrapper->shellSurface()->isMinimized() ? 1 : 0;
    g_state.wrapper_maximized = g_wrapper && g_wrapper->isMaximized() ? 1 : 0;
    g_state.wrapper_fullscreen = g_wrapper
        && g_wrapper->surfaceState() == SurfaceWrapper::State::Fullscreen ? 1 : 0;
    g_state.wrapper_activated = g_wrapper && g_wrapper->isActivated() ? 1 : 0;
    auto *seatContainer = Helper::instance()->rootSurfaceContainer()->getSeatContainerOrDefault();
    g_state.wrapper_focused = seatContainer && seatContainer->keyboardFocusSurface() == g_wrapper ? 1 : 0;
    g_state.wrapper_skip_dock_preview = g_wrapper && g_wrapper->skipDockPreView() ? 1 : 0;
    if (g_wrapper) {
        g_state.wrapper_x = static_cast<int>(g_wrapper->x());
        g_state.wrapper_y = static_cast<int>(g_wrapper->y());
        const QRect icon = g_wrapper->iconGeometry();
        g_state.icon_x = icon.x();
        g_state.icon_y = icon.y();
        g_state.icon_width = icon.width();
        g_state.icon_height = icon.height();
    }
    *static_cast<struct ftm_server_state *>(data) = g_state;
}

extern "C" void ftm_render_and_settle(void *)
{
    if (!g_wrapper)
        return;

    auto *helper = Helper::instance();
    // The protocol request can be queued until the next scene-graph frame.
    // Render first, then inspect the wrapper so we do not miss an animation
    // created while processing that frame.
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

    // Keep the QML animation alive until it has completed. This is an event
    // handshake, not a time-based wait.
    QEventLoop eventLoop;
    QObject::connect(geometryAnimation, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();
}
