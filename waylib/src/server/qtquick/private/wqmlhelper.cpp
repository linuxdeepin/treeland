// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wqmlhelper_p.h"

#include <QSGNode>
#include <QQuickItem>
#include <QCursor>
#include <private/qquickitem_p.h>

#include "private/wprivateaccessor_p.h"

W_DECLARE_PRIVATE_MEMBER(QSGNode_m_subtreeRenderableCount, QSGNode, m_subtreeRenderableCount, int);
W_DECLARE_PRIVATE_MEMBER(QSGNode_m_firstChild, QSGNode, m_firstChild, QSGNode*);
W_DECLARE_PRIVATE_MEMBER(QSGNode_m_lastChild, QSGNode, m_lastChild, QSGNode*);
W_DECLARE_PRIVATE_MEMBER(QSGNode_m_parent, QSGNode, m_parent, QSGNode*);
W_DECLARE_PRIVATE_CONST_METHOD(QImage_metric_tag, QImage, metric, int, QPaintDevice::PaintDeviceMetric);

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
    return W_PRIVATE_CALL(*image, QImage_metric_tag{}, metric);
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
    if (children.last() == item)
        return;
    item->stackAfter(children.last());
}

void WQmlHelper::setCursorShape(QQuickItem *item, WGlobal::CursorShape shape)
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

    if (pos.y() > rect.bottom() - vEdgeSize)
        return Qt::BottomEdge;

    return Qt::Edges::fromInt(0);
}

QSizeF WQmlHelper::scaleSize(const QSizeF &from, const QSizeF &to, Qt::AspectRatioMode mode)
{
    return from.scaled(to, mode);
}

QSGRootNode *WQmlHelper::getRootNode(QQuickItem *item)
{
    const auto d = QQuickItemPrivate::get(item);
    QSGNode *root = d->itemNode();
    if (!root)
        return nullptr;

    while (root->firstChild() && root->type() != QSGNode::RootNodeType)
        root = root->firstChild();
    return root->type() == QSGNode::RootNodeType ? static_cast<QSGRootNode*>(root) : nullptr;
}

int &WQmlHelper::QSGNode_subtreeRenderableCount(QSGNode *node)
{
    return W_PRIVATE_MEMBER(*node, QSGNode_m_subtreeRenderableCount{});
}

QSGNode *&WQmlHelper::QSGNode_firstChild(QSGNode *node)
{
    return W_PRIVATE_MEMBER(*node, QSGNode_m_firstChild{});
}

QSGNode *&WQmlHelper::QSGNode_lastChild(QSGNode *node)
{
    return W_PRIVATE_MEMBER(*node, QSGNode_m_lastChild{});
}

QSGNode *&WQmlHelper::QSGNode_parent(QSGNode *node)
{
    return W_PRIVATE_MEMBER(*node, QSGNode_m_parent{});
}

WAYLIB_SERVER_END_NAMESPACE
