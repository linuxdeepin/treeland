// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"
#include "treeland-foreign-toplevel-manager-v3.h"
#include "workspace/workspace.h"
#include "output/output.h"

#include <wbackend.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>

#include <QEventLoop>
#include <QQuickItem>

#include <cstring>

namespace {
ForeignToplevelManagerInterfaceV3 *g_manager = nullptr;
SurfaceWrapper *g_wrapper = nullptr;
struct ftm_server_state g_state {};
}

void protocol_test_setup(WaylandServer *server)
{
    // Zero duration races GeometryAnimation::finished with the frame snapshot
    // that emits ready(). A short real animation preserves the production
    // lifecycle while the test waits for its finished signal below.
    server->setAnimationSpeed(0.1f);
    add_headless_output(server->backend(), false);
    g_manager = server->shellHandler()->foreignToplevel();
    QObject::connect(g_manager,
                     &ForeignToplevelManagerInterfaceV3::requestDockPreview,
                     server,
                     [](auto surfaces, WSurface *, QPoint abs, auto direction, auto layout) {
                         g_state.preview_fired = 1;
                         g_state.preview_x = abs.x();
                         g_state.preview_y = abs.y();
                         g_state.preview_direction = static_cast<uint32_t>(direction);
                         g_state.preview_layout = static_cast<uint32_t>(layout);
                         g_state.preview_surface_count = static_cast<int>(surfaces.size());
                     });
    QObject::connect(g_manager,
                     &ForeignToplevelManagerInterfaceV3::requestDockPreviewTooltip,
                     server,
                     [](WSurface *surface, WSurface *, QPoint abs, auto direction) {
                         g_state.tooltip_fired = 1;
                         g_state.tooltip_count++;
                         g_state.tooltip_surface_present = surface ? 1 : 0;
                         if (surface) {
                             const QSize size = surface->size();
                             g_state.tooltip_width = size.width();
                             g_state.tooltip_height = size.height();
                         }
                         g_state.tooltip_x = abs.x();
                         g_state.tooltip_y = abs.y();
                         g_state.tooltip_direction = static_cast<uint32_t>(direction);
                     });
    QObject::connect(g_manager,
                     &ForeignToplevelManagerInterfaceV3::requestDockClose,
                     server,
                     [] { g_state.close_fired = 1; });
    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [server](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = server->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                         g_state.mapped_xdg_toplevel = wrapper->surface() && wrapper->surface()->mapped() ? 1 : 0;
                     });
}

extern "C" void ftm_read_server_state(void *data)
{
    g_state.output_ready = !WaylandServer::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    g_state.wrapper_minimized = g_wrapper && g_wrapper->shellSurface()
        && g_wrapper->shellSurface()->isMinimized() ? 1 : 0;
    g_state.wrapper_self_minimized = g_wrapper && g_wrapper->isMinimized() ? 1 : 0;
    g_state.wrapper_state =
        g_wrapper ? static_cast<int>(g_wrapper->surfaceState()) : -1;
    g_state.wrapper_visible = g_wrapper && g_wrapper->isVisible() ? 1 : 0;
    g_state.wrapper_maximized = g_wrapper && g_wrapper->isMaximized() ? 1 : 0;
    g_state.wrapper_fullscreen = g_wrapper
        && g_wrapper->surfaceState() == SurfaceWrapper::State::Fullscreen ? 1 : 0;
    g_state.wrapper_activated = g_wrapper && g_wrapper->isActivated() ? 1 : 0;
    auto *seatContainer = WaylandServer::instance()->rootSurfaceContainer()->getSeatContainerOrDefault();
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

    auto *server = WaylandServer::instance();
    // The protocol request can be queued until the next scene-graph frame.
    // Render first, then inspect the wrapper so we do not miss an animation
    // created while processing that frame.
    for (auto *output : server->rootSurfaceContainer()->outputs()) {
        if (auto *vp = output->screenViewport())
            vp->render(true);
    }
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
