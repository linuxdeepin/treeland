// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-decoration-unstable-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"
#include "surface/surfaceproxy.h"
#include "desktop/desktopenvironment.h"
#include "treelanduserconfig.hpp"

#include <wbackend.h>
#include <protocols/wxdgdecorationmanager.h>

#include <QQuickWindow>
#include <QSignalSpy>

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

    auto *owner = g_wrapper->desktop();
    if (!owner)
        return;
    const auto windowCount = owner->windows()->rowCount();
    const auto surfaceCount = owner->surfaces()->rowCount();
    auto *config = owner->config();
    const auto titlebarHeight = config->windowTitlebarHeight();
    config->setWindowTitlebarHeight(56);
    {
        // Exercise real proxy geometry and preferences without registering a second window.
        SurfaceProxy proxy;
        proxy.setSize(QSizeF(200, 150));
        proxy.setRadius(12);
        proxy.setSurface(g_wrapper);
        proxy.setFullProxy(true);
        auto *proxyWrapper = proxy.findChild<SurfaceWrapper *>();
        state->full_proxy_allows_rounding = proxyWrapper && !proxyWrapper->noCornerRadius()
            && proxyWrapper->radius() > 0;
        if (proxyWrapper) {
            state->proxy_uses_source_configuration = proxyWrapper->desktop() == owner;
            if (g_wrapper->titleBar()) {
                state->proxy_uses_source_configuration &= proxyWrapper->titleBar()
                    && proxyWrapper->titleBar()->height() == 56;
                config->setWindowTitlebarHeight(72);
                state->proxy_uses_source_configuration &= proxyWrapper->titleBar()
                    && proxyWrapper->titleBar()->height() == 72;
            }
            state->proxy_uses_source_configuration &= owner->windows()->rowCount() == windowCount
                && owner->surfaces()->rowCount() == surfaceCount;

            QSignalSpy radiusChanges(&proxy, &SurfaceProxy::radiusChanged);
            proxy.setPosition(QPointF(10, 20));
            proxy.setHeight(proxy.height() + 10);
            proxy.setSize(proxyWrapper->size() * 0.5);
            state->proxy_radius_notifications_valid = radiusChanges.isEmpty()
                && proxy.radius() == 12;

            proxy.resetRadius();
            radiusChanges.clear();
            const qreal previousRadius = proxy.radius();
            proxy.setSize(proxyWrapper->size() * 0.25);
            state->proxy_radius_notifications_valid &= radiusChanges.size()
                == (qFuzzyCompare(previousRadius, proxy.radius()) ? 0 : 1);
            radiusChanges.clear();
            proxy.setPosition(QPointF(20, 30));
            state->proxy_radius_notifications_valid &= radiusChanges.isEmpty();

            const bool noDecoration = g_wrapper->noDecoration();
            g_wrapper->setNoDecoration(!noDecoration);
            const qreal expectedScale = qMin(qMin(proxy.width() / proxyWrapper->width(),
                                                 proxy.height() / proxyWrapper->height()), qreal(1));
            state->proxy_radius_notifications_valid &= qFuzzyCompare(proxyWrapper->scale(), expectedScale);
            g_wrapper->setNoDecoration(noDecoration);
            radiusChanges.clear();

            // State changes can alter a CSD radius without changing the proxy scale.
            g_wrapper->setSurfaceStateDirectly(SurfaceWrapper::State::Maximized);
            state->proxy_radius_notifications_valid &= proxy.radius() == g_wrapper->radius()
                && !radiusChanges.isEmpty();
            radiusChanges.clear();
            g_wrapper->setSurfaceStateDirectly(SurfaceWrapper::State::Normal);
            state->proxy_radius_notifications_valid &= proxy.radius()
                == (g_wrapper->noDecoration() ? 0 : g_wrapper->radius() * proxyWrapper->scale())
                && !radiusChanges.isEmpty();

            proxy.setSize(QSizeF());
            radiusChanges.clear();
            g_wrapper->setSurfaceStateDirectly(SurfaceWrapper::State::Maximized);
            state->proxy_radius_notifications_valid &= proxy.radius() == g_wrapper->radius()
                && !radiusChanges.isEmpty();
            g_wrapper->setSurfaceStateDirectly(SurfaceWrapper::State::Normal);

            proxy.setRadius(0);
            state->proxy_zero_radius_preserved = proxyWrapper->radius() == 0;
            proxy.resetRadius();
            if (g_wrapper->noDecoration())
                state->proxy_zero_radius_preserved &= proxyWrapper->radius() == 0;

            const qreal detachedRadius = proxy.radius();
            radiusChanges.clear();
            proxy.setSurface(nullptr);
            state->proxy_radius_notifications_valid &= proxy.radius() == 0
                && radiusChanges.size() == (qFuzzyIsNull(detachedRadius) ? 0 : 1);
        }
    }
    config->setWindowTitlebarHeight(titlebarHeight);
    state->proxy_uses_source_configuration &= owner->windows()->rowCount() == windowCount
        && owner->surfaces()->rowCount() == surfaceCount;
}
