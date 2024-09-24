// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "wsurface.h"

#include <qwxdgshell.h>
#include <qwseat.h>

#include <QCoreApplication>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgSurfaceItemPrivate : public WSurfaceItemPrivate
{
    Q_DECLARE_PUBLIC(WXdgSurfaceItem)
public:
    void setImplicitPosition(const QPointF &newImplicitPosition);

public:
    QPointF implicitPosition;
    QSize minimumSize;
    QSize maximumSize;
};

WXdgSurfaceItem::WXdgSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WXdgSurfaceItemPrivate(), parent)
{

}

WXdgSurfaceItem::~WXdgSurfaceItem()
{

}

WXdgSurface* WXdgSurfaceItem::xdgSurface() const
{
    return qobject_cast<WXdgSurface*>(shellSurface());
}

QPointF WXdgSurfaceItem::implicitPosition() const
{
    const Q_D(WXdgSurfaceItem);
    return d->implicitPosition;
}

QSize WXdgSurfaceItem::maximumSize() const
{
    const Q_D(WXdgSurfaceItem);
    return d->maximumSize;
}

QSize WXdgSurfaceItem::minimumSize() const
{
    const Q_D(WXdgSurfaceItem);
    return d->minimumSize;
}

inline static int32_t getValidSize(int32_t size, int32_t fallback) {
    return size > 0 ? size : fallback;
}

void WXdgSurfaceItem::onSurfaceCommit()
{
    Q_D(WXdgSurfaceItem);

    WSurfaceItem::onSurfaceCommit();
    if (auto popup = xdgSurface()->handle()->handle()->popup) {
        Q_UNUSED(popup);
        d->setImplicitPosition(xdgSurface()->getPopupPosition());
    } else if (auto toplevel = xdgSurface()->handle()->handle()->toplevel) {
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
    }
}

void WXdgSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(xdgSurface());
    connect(xdgSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WXdgSurfaceItem::releaseResources);
}

QRectF WXdgSurfaceItem::getContentGeometry() const
{
    return xdgSurface()->getContentGeometry();
}

void WXdgSurfaceItemPrivate::setImplicitPosition(const QPointF &newImplicitPosition)
{
    Q_Q(WXdgSurfaceItem);

    if (implicitPosition == newImplicitPosition)
        return;
    implicitPosition = newImplicitPosition;
    Q_EMIT q->implicitPositionChanged();
}

WAYLIB_SERVER_END_NAMESPACE
