// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wxwaylandsurface.h"
#include "private/wtoplevelsurface_p.h"
#include "wscoplistener.h"

#include <wlr_fwd.h>

#include <climits>
#include <QPointer>
#include <QRect>
#include <QSize>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXWaylandSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    WXWaylandSurfacePrivate(WXWaylandSurface *qq, wlr_xwayland_surface *handle, WXWayland *xwayland);
    ~WXWaylandSurfacePrivate();

    inline static WXWaylandSurfacePrivate *get(WXWaylandSurface *q) {
        return q->d_func();
    }

    inline wlr_xwayland_surface *handle() const {
        return m_handle;
    }

    inline bool isMaximized() const {
        return handle()->maximized_horz && handle()->maximized_vert;
    }

    wl_client *waylandClient() const override {
        return surface->handle()->resource->client;
    }

    void init();
    void updateChildren();
    void updateParent();
    void updateSizeHints();
    void updateWindowTypes();
    // Called by the owner (WXWayland) while the native parent surface is
    // being destroyed, before the parent wrapper is deleted: the child's
    // QPointer to the parent wrapper would otherwise be cleared by the
    // deletion before the native set_parent event arrives, making the
    // regular parent-loss handling skip the change.
    void handleParentDestroyed(WXWaylandSurface *parent);

    W_DECLARE_PUBLIC(WXWaylandSurface)

    WSurface *surface = nullptr;
    WXWayland *xwayland = nullptr;
    mutable int pidFD = -1;

    QList<WXWaylandSurface*> children;
    QPointer<WXWaylandSurface> parent;
    QRect lastRequestConfigureGeometry;
    WXWaylandSurface::ConfigureFlags lastRequestConfigureFlags = {0};
    WXWaylandSurface::WindowTypes windowTypes = {0};
    QSize minimumSize;
    QSize maximumSize = QSize(INT_MAX, INT_MAX);
    uint maximized:1;
    uint minimized:1;
    uint fullscreen:1;
    uint activated:1;
    bool x11Mapped = false;
    quint64 x11MapGeneration = 0;

private:
    // XWayland owns this handle and destroys it after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_xwayland_surface *m_handle = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
