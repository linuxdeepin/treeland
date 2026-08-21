// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtoplevelsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wsurfaceitem.h"
#include "wxdgshell.h"
#include "wxdgtoplevelsurface.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgToplevelSurfaceItemPrivate : public WSurfaceItemPrivate
{
    Q_DECLARE_PUBLIC(WXdgToplevelSurfaceItem)
public:

public:
    QSize minimumSize;
    QSize maximumSize;
};

WXdgToplevelSurfaceItem::WXdgToplevelSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WXdgToplevelSurfaceItemPrivate(), parent)
{

}

WXdgToplevelSurfaceItem::~WXdgToplevelSurfaceItem()
{

}

WXdgToplevelSurface *WXdgToplevelSurfaceItem::toplevelSurface() const
{
    return qobject_cast<WXdgToplevelSurface*>(shellSurface());
}

QSize WXdgToplevelSurfaceItem::maximumSize() const
{
    const Q_D(WXdgToplevelSurfaceItem);
    return d->maximumSize;
}

QSize WXdgToplevelSurfaceItem::minimumSize() const
{
    const Q_D(WXdgToplevelSurfaceItem);
    return d->minimumSize;
}

inline static int32_t getValidSize(int32_t size, int32_t fallback) {
    return size > 0 ? size : fallback;
}

void WXdgToplevelSurfaceItem::onSurfaceCommit()
{
    Q_D(WXdgToplevelSurfaceItem);

    WSurfaceItem::onSurfaceCommit();

    auto toplevel = toplevelSurface()->handle();
    const QSize minSize(getValidSize(toplevel->current.min_width, 0),
                        getValidSize(toplevel->current.min_height, 0));
    const QSize maxSize(getValidSize(toplevel->current.max_width, INT_MAX),
                        getValidSize(toplevel->current.max_height, INT_MAX));

    if (d->minimumSize != minSize) {
        d->minimumSize = minSize;
        Q_EMIT minimumSizeChanged();
    }

    if (d->maximumSize != maxSize) {
        d->maximumSize = maxSize;
        Q_EMIT maximumSizeChanged();
    }

    auto xdg_surface = toplevelSurface()->handle()->base;
    if (xdg_surface->initial_commit &&
        !toplevelSurface()->handle()->requested.maximized &&
        !toplevelSurface()->handle()->requested.fullscreen) {
        /* When an xdg_surface performs an initial commit, the compositor must
         * reply with a configure so the client can map the surface.
         * configures the xdg_toplevel with 0,0 size to let the client pick the
         * dimensions itself. */
        wlr_xdg_toplevel_set_size(toplevelSurface()->handle(), 0, 0);
    }
}

void WXdgToplevelSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(toplevelSurface());
    connect(toplevelSurface(), &WToplevelSurface::beforeDestroy,
            this, &WXdgToplevelSurfaceItem::releaseResources);
}

QRectF WXdgToplevelSurfaceItem::getContentGeometry() const
{
    return toplevelSurface()->getContentGeometry();
}

WAYLIB_SERVER_END_NAMESPACE
