// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-decoration-unstable-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"
#include "surface/surfaceproxy.h"

#include <wbackend.h>
#include <protocols/wxdgdecorationmanager.h>

#include <QQuickWindow>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
SurfaceWrapper *g_wrapper = nullptr;
WXdgDecorationManager *g_decoMgr = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    g_decoMgr = find_server_interface<WXdgDecorationManager>(server);

    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

void xdg_decoration_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_decoration_server_state *>(data);
    *state = {};

    if (!g_wrapper)
        return;

    auto *wsurface = g_wrapper->surface();
    if (!wsurface)
        return;

    state->valid = 1;
    state->decorated = g_wrapper->decoration() && g_wrapper->visibleDecoration()
        && !g_wrapper->noCornerRadius() && !g_wrapper->frameGeometry().rect().isEmpty();
    state->requested_radius = g_wrapper->radius();
    state->frame_radius = g_wrapper->frameGeometry().radius();
    auto *window = g_wrapper->window();
    state->software_renderer = window
        && window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
    auto *item = g_wrapper->surfaceItem();
    state->has_input_mask = item && item->eventItem() && item->eventItem()->containmentMask();

    if (g_decoMgr) {
        state->mode = static_cast<int>(g_decoMgr->modeBySurface(wsurface));
    }

    // Switching to a full CSD proxy must not suppress its requested thumbnail radius.
    SurfaceProxy proxy;
    proxy.setSize(QSizeF(200, 150));
    proxy.setRadius(12);
    proxy.setSurface(g_wrapper);
    proxy.setFullProxy(true);
    auto *proxyWrapper = proxy.findChild<SurfaceWrapper *>();
    state->full_proxy_allows_rounding = proxyWrapper && !proxyWrapper->noCornerRadius()
        && proxyWrapper->radius() > 0;
}
