// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wqmlhelper_p.h"

#include <QQuickItem>
#include <QCursor>

WAYLIB_SERVER_BEGIN_NAMESPACE

WImageRenderTarget::WImageRenderTarget()
    : image(new QImage())
{

}

int WImageRenderTarget::devType() const
{
    return QInternal::CustomRaster;
}

QPaintEngine *WImageRenderTarget::paintEngine() const
{
    return image->paintEngine();
}

void WImageRenderTarget::setDevicePixelRatio(qreal dpr)
{
    image->setDevicePixelRatio(dpr);
}

int WImageRenderTarget::metric(PaintDeviceMetric metric) const
{
    return image->metric(metric);
}

QPaintDevice *WImageRenderTarget::redirected(QPoint *offset) const
{
    if (offset)
        *offset = QPoint(0, 0);

    return image.get();
}

WQmlHelper::WQmlHelper(QObject *parent)
    : QObject{parent}
{

}

bool WQmlHelper::hasXWayland() const
{
#ifdef DISABLE_XWAYLAND
    return false;
#else
    return true;
#endif
}

void WQmlHelper::itemStackToTop(QQuickItem *item)
{
    auto parent = item->parentItem();
    if (!parent)
        return;
    auto children = parent->childItems();
    if (children.count() < 2)
        return;
    item->stackAfter(children.last());
}

void WQmlHelper::setCursorShape(QQuickItem *item, WCursor::CursorShape shape)
{
    item->setCursor(WCursor::toQCursor(shape));
}

Qt::Edges WQmlHelper::getEdges(const QRectF &rect, const QPointF &pos, qreal edgeSize)
{
    const qreal vEdgeSize = std::min(edgeSize, rect.height() / 3);
    const qreal hEdgeSize = std::min(edgeSize, rect.width() / 3);

    if (pos.x() < rect.x() + hEdgeSize) {
        if (pos.y() < rect.y() + vEdgeSize)
            return Qt::TopEdge | Qt::LeftEdge;
        if (pos.y() > rect.bottom() - vEdgeSize)
            return Qt::BottomEdge | Qt::LeftEdge;

        return Qt::LeftEdge;
    } else if (pos.x() > rect.right() - hEdgeSize) {
        if (pos.y() < rect.y() + vEdgeSize)
            return Qt::TopEdge | Qt::RightEdge;
        if (pos.y() > rect.bottom() - vEdgeSize)
            return Qt::BottomEdge | Qt::RightEdge;

        return Qt::RightEdge;
    }

    if (pos.y() < rect.y() + vEdgeSize)
        return Qt::TopEdge;

    return Qt::BottomEdge;
}

WAYLIB_SERVER_END_NAMESPACE
