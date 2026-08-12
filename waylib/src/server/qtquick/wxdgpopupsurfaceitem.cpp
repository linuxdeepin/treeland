// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgpopupsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wxdgpopupsurface.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgPopupSurfaceItemPrivate : public WSurfaceItemPrivate
{
    Q_DECLARE_PUBLIC(WXdgPopupSurfaceItem)
public:
    void setImplicitPosition(const QPointF &newImplicitPosition);

public:
    QPointF implicitPosition;
};

WXdgPopupSurfaceItem::WXdgPopupSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WXdgPopupSurfaceItemPrivate(), parent)
{

}

WXdgPopupSurfaceItem::~WXdgPopupSurfaceItem()
{

}

WXdgPopupSurface *WXdgPopupSurfaceItem::popupSurface() const
{
    return qobject_cast<WXdgPopupSurface*>(shellSurface());
}

QPointF WXdgPopupSurfaceItem::implicitPosition() const
{
    const Q_D(WXdgPopupSurfaceItem);
    return d->implicitPosition;
}

void WXdgPopupSurfaceItem::onSurfaceCommit()
{
    Q_D(WXdgPopupSurfaceItem);

    WSurfaceItem::onSurfaceCommit();
    d->setImplicitPosition(popupSurface()->getPopupPosition());

    auto xdg_surface = popupSurface()->handle()->base;
    if (xdg_surface->initial_commit) {
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }
}

void WXdgPopupSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(popupSurface());
    connect(popupSurface(), &WToplevelSurface::beforeDestroy,
            this, &WXdgPopupSurfaceItem::releaseResources);
}

QRectF WXdgPopupSurfaceItem::getContentGeometry() const
{
    return popupSurface()->getContentGeometry();
}

void WXdgPopupSurfaceItemPrivate::setImplicitPosition(const QPointF &newImplicitPosition)
{
    Q_Q(WXdgPopupSurfaceItem);

    if (implicitPosition == newImplicitPosition)
        return;
    implicitPosition = newImplicitPosition;
    Q_EMIT q->implicitPositionChanged();
}

WAYLIB_SERVER_END_NAMESPACE
