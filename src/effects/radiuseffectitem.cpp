// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "radiuseffectitem.h"
#include "radiuseffectitem_p.h"
#include <wradiustexturenode_p.h>

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGTextureProvider>
#include <QtQml/qqmlinfo.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace Treeland {

RadiusEffectItem::RadiusEffectItem(QQuickItem *parent)
    : QQuickItem(*(new RadiusEffectItemPrivate), parent)
{
    setFlag(ItemHasContents);
}

RadiusEffectItem::~RadiusEffectItem()
{
    Q_D(RadiusEffectItem);

    if (!d->sourceItem)
        return;

    auto sourcePrivate = QQuickItemPrivate::get(d->sourceItem);
    sourcePrivate->derefFromEffectItem(d->hideSource);
    if (window())
        sourcePrivate->derefWindow();
}

qreal RadiusEffectItem::radius() const
{
    Q_D(const RadiusEffectItem);

    return d->radius;
}

void RadiusEffectItem::setRadius(qreal radius)
{
    Q_D(RadiusEffectItem);

    radius = qMax<qreal>(0.0, radius);
    if (d->radius == radius)
        return;

    d->radius = radius;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT radiusChanged();
    if (d->topLeftRadius < 0.0)
        Q_EMIT topLeftRadiusChanged();
    if (d->topRightRadius < 0.0)
        Q_EMIT topRightRadiusChanged();
    if (d->bottomLeftRadius < 0.0)
        Q_EMIT bottomLeftRadiusChanged();
    if (d->bottomRightRadius < 0.0)
        Q_EMIT bottomRightRadiusChanged();
}

qreal RadiusEffectItem::topLeftRadius() const
{
    Q_D(const RadiusEffectItem);

    if (d->topLeftRadius >= 0.0)
        return d->topLeftRadius;

    return d->radius;
}

void RadiusEffectItem::setTopLeftRadius(qreal radius)
{
    Q_D(RadiusEffectItem);

    if (d->topLeftRadius == radius)
        return;

    if (radius < 0.0) {
        qmlWarning(this) << "TopLeftRadius cannot be less than zero";
        return;
    }

    d->topLeftRadius = radius;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT topLeftRadiusChanged();
}

void RadiusEffectItem::resetTopLeftRadius()
{
    Q_D(RadiusEffectItem);

    if (d->topLeftRadius < 0.0)
        return;

    d->topLeftRadius = -1.0;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT topLeftRadiusChanged();
}

qreal RadiusEffectItem::topRightRadius() const
{
    Q_D(const RadiusEffectItem);

    if (d->topRightRadius >= 0.0)
        return d->topRightRadius;
    return d->radius;
}

void RadiusEffectItem::setTopRightRadius(qreal radius)
{
    Q_D(RadiusEffectItem);

    if (d->topRightRadius == radius)
        return;

    if (radius < 0.0) {
        qmlWarning(this) << "TopRightRadius cannot be less than zero";
        return;
    }

    d->topRightRadius = radius;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT topRightRadiusChanged();
}

void RadiusEffectItem::resetTopRightRadius()
{
    Q_D(RadiusEffectItem);

    if (d->topRightRadius < 0.0)
        return;

    d->topRightRadius = -1.0;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT topRightRadiusChanged();
}

qreal RadiusEffectItem::bottomLeftRadius() const
{
    Q_D(const RadiusEffectItem);

    if (d->bottomLeftRadius >= 0.0)
        return d->bottomLeftRadius;

    return d->radius;
}

void RadiusEffectItem::setBottomLeftRadius(qreal radius)
{
    Q_D(RadiusEffectItem);

    if (d->bottomLeftRadius == radius)
        return;

    if (radius < 0.0) {
        qmlWarning(this) << "BottomLeftRadius cannot be less than zero";
        return;
    }

    d->bottomLeftRadius = radius;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT bottomLeftRadiusChanged();
}

void RadiusEffectItem::resetBottomLeftRadius()
{
    Q_D(RadiusEffectItem);

    if (d->bottomLeftRadius < 0.0)
        return;

    d->bottomLeftRadius = -1.0;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT bottomLeftRadiusChanged();
}

qreal RadiusEffectItem::bottomRightRadius() const
{
    Q_D(const RadiusEffectItem);

    if (d->bottomRightRadius >= 0.0)
        return d->bottomRightRadius;

    return d->radius;
}

void RadiusEffectItem::setBottomRightRadius(qreal radius)
{
    Q_D(RadiusEffectItem);

    if (d->bottomRightRadius == radius)
        return;

    if (radius < 0.0) {
        qmlWarning(this) << "BottomRightRadius cannot be less than zero";
        return;
    }

    d->bottomRightRadius = radius;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT bottomRightRadiusChanged();
}

void RadiusEffectItem::resetBottomRightRadius()
{
    Q_D(RadiusEffectItem);

    if (d->bottomRightRadius < 0.0)
        return;

    d->bottomRightRadius = -1.0;
    d->updateImplicitAntialiasing();
    update();
    Q_EMIT bottomRightRadiusChanged();
}

QQuickItem *RadiusEffectItem::sourceItem() const
{
    Q_D(const RadiusEffectItem);

    return d->sourceItem;
}

void RadiusEffectItem::setSourceItem(QQuickItem *item)
{
    Q_D(RadiusEffectItem);

    if (item == d->sourceItem)
        return;

    if (d->sourceItem) {
        auto oldPrivate = QQuickItemPrivate::get(d->sourceItem);
        oldPrivate->derefFromEffectItem(d->hideSource);
        if (window())
            oldPrivate->derefWindow();
        disconnect(d->sourceItem, &QObject::destroyed,
                   this, &RadiusEffectItem::sourceItemDestroyed);
    }

    d->sourceItem = item;
    if (d->sourceItem) {
        if (window() && d->sourceItem->window() && window() != d->sourceItem->window()) {
            qmlWarning(this) << "sourceItem and RadiusEffect must belong to the same window";
            d->sourceItem = nullptr;
        } else {
            auto sourcePrivate = QQuickItemPrivate::get(d->sourceItem);
            if (window())
                sourcePrivate->refWindow(window());
            else if (d->sourceItem->window())
                sourcePrivate->refWindow(d->sourceItem->window());
            sourcePrivate->refFromEffectItem(d->hideSource);
            connect(d->sourceItem, &QObject::destroyed,
                    this, &RadiusEffectItem::sourceItemDestroyed);
        }
    }

    update();
    Q_EMIT sourceItemChanged();
}

void RadiusEffectItem::sourceItemDestroyed(QObject *item)
{
    Q_D(RadiusEffectItem);

    Q_ASSERT(item == d->sourceItem);
    d->sourceItem = nullptr;
    update();
    Q_EMIT sourceItemChanged();
}

bool RadiusEffectItem::hideSource() const
{
    Q_D(const RadiusEffectItem);

    return d->hideSource;
}

void RadiusEffectItem::setHideSource(bool hide)
{
    Q_D(RadiusEffectItem);

    if (d->hideSource == hide)
        return;
    if (d->sourceItem) {
        auto sourcePrivate = QQuickItemPrivate::get(d->sourceItem);
        sourcePrivate->derefFromEffectItem(d->hideSource);
        sourcePrivate->refFromEffectItem(hide);
    }
    d->hideSource = hide;
    Q_EMIT hideSourceChanged();
}

QPointF RadiusEffectItem::sourceOffset() const
{
    Q_D(const RadiusEffectItem);

    return d->sourceOffset;
}

void RadiusEffectItem::setSourceOffset(const QPointF &offset)
{
    Q_D(RadiusEffectItem);

    if (d->sourceOffset == offset)
        return;

    d->sourceOffset = offset;
    update();
    Q_EMIT sourceOffsetChanged();
}

qreal RadiusEffectItem::sourceWidthScale() const
{
    Q_D(const RadiusEffectItem);

    return d->sourceWidthScale;
}

void RadiusEffectItem::setSourceWidthScale(qreal scale)
{
    Q_D(RadiusEffectItem);

    if (qFuzzyCompare(d->sourceWidthScale, scale))
        return;
    d->sourceWidthScale = scale;
    update();
    Q_EMIT sourceWidthScaleChanged();
}

qreal RadiusEffectItem::sourceHeightScale() const
{
    Q_D(const RadiusEffectItem);

    return d->sourceHeightScale;
}

void RadiusEffectItem::setSourceHeightScale(qreal scale)
{
    Q_D(RadiusEffectItem);

    if (qFuzzyCompare(d->sourceHeightScale, scale))
        return;
    d->sourceHeightScale = scale;
    update();
    Q_EMIT sourceHeightScaleChanged();
}

QRectF RadiusEffectItem::renderRect() const
{
    Q_D(const RadiusEffectItem);

    return d->renderRect;
}

void RadiusEffectItem::setRenderRect(const QRectF &rect)
{
    Q_D(RadiusEffectItem);

    if (d->renderRect == rect)
        return;

    d->renderRect = rect;
    update();
    Q_EMIT renderRectChanged();
}

void RadiusEffectItem::resetRenderRect()
{
    setRenderRect(QRectF());
}

QSGNode *RadiusEffectItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    Q_D(RadiusEffectItem);

    if (!d->sourceItem || d->sourceItem->width() <= 0.0 || d->sourceItem->height() <= 0.0
            || width() <= 0.0 || height() <= 0.0 || !d->sourceItem->isTextureProvider()) {
        delete oldNode;
        return nullptr;
    }

    if (!window()
            || window()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<RadiusTextureNode *>(oldNode);
    if (!node)
        node = new RadiusTextureNode;

    node->setTextureProvider(d->sourceItem->textureProvider());
    node->setRect(d->renderRect.isNull() ? boundingRect() : d->renderRect);
    node->setSourceOffset(d->sourceOffset);
    node->setSourceSizeScale(d->sourceWidthScale, d->sourceHeightScale);
    node->setRadius(d->radius);
    node->setTopLeftRadius(d->topLeftRadius);
    node->setTopRightRadius(d->topRightRadius);
    node->setBottomLeftRadius(d->bottomLeftRadius);
    node->setBottomRightRadius(d->bottomRightRadius);
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    node->setMipmapFiltering(QSGTexture::None);
    node->setAnisotropyLevel(QSGTexture::AnisotropyNone);
    node->setTextureCoordinatesTransform(QSGImageNode::NoTransform);
    node->setAntialiasing(antialiasing());

    return node;
}

void RadiusEffectItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(RadiusEffectItem);

    if (change == ItemSceneChange && d->sourceItem) {
        auto sourcePrivate = QQuickItemPrivate::get(d->sourceItem);
        if (value.window)
            sourcePrivate->refWindow(value.window);
        else
            sourcePrivate->derefWindow();
    }
    QQuickItem::itemChange(change, value);
}

} // namespace Treeland
