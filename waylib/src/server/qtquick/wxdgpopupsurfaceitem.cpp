// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgpopupsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wxdgpopupsurface.h"

#include <qwxdgshell.h>
#include <qwseat.h>

QW_USE_NAMESPACE
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

    auto xdg_surface = popupSurface()->handle()->handle()->base;
    if (xdg_surface->initial_commit) {
        qw_xdg_surface::from(xdg_surface)->schedule_configure();
    }
}

void WXdgPopupSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(popupSurface());
    connect(popupSurface(), &WWrapObject::aboutToBeInvalidated,
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
