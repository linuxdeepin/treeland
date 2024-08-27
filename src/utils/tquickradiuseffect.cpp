// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "tquickradiuseffect.h"
#include "tquickradiuseffect_p.h"
#include "tsgradiusimagenode.h"

void TQuickRadiusEffectPrivate::maybeSetImplicitAntialiasing()
{
    bool implicitAA = (radius != 0);
    if (extraRadius.isAllocated() && !implicitAA) {
        implicitAA = extraRadius.value().topLeftRadius > 0.0
            || extraRadius.value().topRightRadius > 0.0
            || extraRadius.value().bottomLeftRadius > 0.0
            || extraRadius.value().bottomRightRadius > 0.0;
    }
    setImplicitAntialiasing(implicitAA);
}

TQuickRadiusEffect::TQuickRadiusEffect(QQuickItem *parent)
    : QQuickItem(*(new TQuickRadiusEffectPrivate), parent)
{
    setFlag(ItemHasContents);
}

TQuickRadiusEffect::~TQuickRadiusEffect()
{
    Q_D(const TQuickRadiusEffect);

    if (d->sourceItem) {
        QQuickItemPrivate *sd = QQuickItemPrivate::get(d->sourceItem);
        sd->derefFromEffectItem(d->hideSource);
        if (window())
            sd->derefWindow();
    }
}

qreal TQuickRadiusEffect::radius() const
{
    Q_D(const TQuickRadiusEffect);

    return d->radius;
}

void TQuickRadiusEffect::setRadius(qreal radius)
{
    Q_D(TQuickRadiusEffect);

    if (d->radius == radius)
        return;

    d->radius = radius;
    d->maybeSetImplicitAntialiasing();

    update();
    emit radiusChanged();

    if (d->extraRadius.isAllocated()) {
        if (d->extraRadius->topLeftRadius < 0.)
            emit topLeftRadiusChanged();
        if (d->extraRadius->topRightRadius < 0.)
            emit topRightRadiusChanged();
        if (d->extraRadius->bottomLeftRadius < 0.)
            emit bottomLeftRadiusChanged();
        if (d->extraRadius->bottomRightRadius < 0.)
            emit bottomRightRadiusChanged();
    } else {
        emit topLeftRadiusChanged();
        emit topRightRadiusChanged();
        emit bottomLeftRadiusChanged();
        emit bottomRightRadiusChanged();
    }
}

qreal TQuickRadiusEffect::topLeftRadius() const
{
    Q_D(const TQuickRadiusEffect);

    if (d->extraRadius.isAllocated() && d->extraRadius->topLeftRadius >= 0.)
        return d->extraRadius.value().topLeftRadius;

    return d->radius;
}

void TQuickRadiusEffect::setTopLeftRadius(qreal radius)
{
    Q_D(TQuickRadiusEffect);

    if (d->extraRadius.value().topLeftRadius == radius)
        return;

    if (radius < 0) { // use the fact that radius < 0 resets the radius.
        qmlWarning(this) << "topLeftRadius (" << radius << ") cannot be less than 0.";
        return;
    }
    d->extraRadius.value().topLeftRadius = radius;
    d->maybeSetImplicitAntialiasing();

    update();
    emit topLeftRadiusChanged();
}

void TQuickRadiusEffect::resetTopLeftRadius()
{
    Q_D(TQuickRadiusEffect);

    if (!d->extraRadius.isAllocated())
        return;
    if (d->extraRadius.value().topLeftRadius < 0)
        return;

    d->extraRadius.value().topLeftRadius = -1.;
    d->maybeSetImplicitAntialiasing();

    update();
    emit topLeftRadiusChanged();
}

qreal TQuickRadiusEffect::topRightRadius() const
{
    Q_D(const TQuickRadiusEffect);

    if (d->extraRadius.isAllocated()  && d->extraRadius->topRightRadius >= 0.)
        return d->extraRadius.value().topRightRadius;

    return d->radius;
}

void TQuickRadiusEffect::setTopRightRadius(qreal radius)
{
    Q_D(TQuickRadiusEffect);

    if (d->extraRadius.value().topRightRadius == radius)
        return;

    if (radius < 0) { // use the fact that radius < 0 resets the radius.
        qmlWarning(this) << "topRightRadius (" << radius << ") cannot be less than 0.";
        return;
    }
    d->extraRadius.value().topRightRadius = radius;
    d->maybeSetImplicitAntialiasing();

    update();
    emit topRightRadiusChanged();
}

void TQuickRadiusEffect::resetTopRightRadius()
{
    Q_D(TQuickRadiusEffect);

    if (!d->extraRadius.isAllocated())
        return;
    if (d->extraRadius.value().topRightRadius < 0)
        return;

    d->extraRadius.value().topRightRadius = -1.;
    d->maybeSetImplicitAntialiasing();

    update();
    emit topRightRadiusChanged();
}

qreal TQuickRadiusEffect::bottomLeftRadius() const
{
    Q_D(const TQuickRadiusEffect);

    if (d->extraRadius.isAllocated() && d->extraRadius->bottomLeftRadius >= 0.)
        return d->extraRadius.value().bottomLeftRadius;
    return d->radius;
}

void TQuickRadiusEffect::setBottomLeftRadius(qreal radius)
{
    Q_D(TQuickRadiusEffect);

    if (d->extraRadius.value().bottomLeftRadius == radius)
        return;

    if (radius < 0) { // use the fact that radius < 0 resets the radius.
        qmlWarning(this) << "bottomLeftRadius (" << radius << ") cannot be less than 0.";
        return;
    }

    d->extraRadius.value().bottomLeftRadius = radius;
    d->maybeSetImplicitAntialiasing();

    update();
    emit bottomLeftRadiusChanged();
}

void TQuickRadiusEffect::resetBottomLeftRadius()
{
    Q_D(TQuickRadiusEffect);

    if (!d->extraRadius.isAllocated())
        return;
    if (d->extraRadius.value().bottomLeftRadius < 0)
        return;

    d->extraRadius.value().bottomLeftRadius = -1.;
    d->maybeSetImplicitAntialiasing();

    update();
    emit bottomLeftRadiusChanged();
}

qreal TQuickRadiusEffect::bottomRightRadius() const
{
    Q_D(const TQuickRadiusEffect);

    if (d->extraRadius.isAllocated() && d->extraRadius->bottomRightRadius >= 0.)
        return d->extraRadius.value().bottomRightRadius;
    return d->radius;
}

void TQuickRadiusEffect::setBottomRightRadius(qreal radius)
{
    Q_D(TQuickRadiusEffect);

    if (d->extraRadius.value().bottomRightRadius == radius)
        return;

    if (radius < 0) { // use the fact that radius < 0 resets the radius.
        qmlWarning(this) << "bottomRightRadius (" << radius << ") cannot be less than 0.";
        return;
    }

    d->extraRadius.value().bottomRightRadius = radius;
    d->maybeSetImplicitAntialiasing();

    update();
    emit bottomRightRadiusChanged();
}

void TQuickRadiusEffect::resetBottomRightRadius()
{
    Q_D(TQuickRadiusEffect);

    if (!d->extraRadius.isAllocated())
        return;
    if (d->extraRadius.value().bottomRightRadius < 0)
        return;

    d->extraRadius.value().bottomRightRadius = -1.;
    d->maybeSetImplicitAntialiasing();

    update();
    emit bottomRightRadiusChanged();
}

QQuickItem *TQuickRadiusEffect::sourceItem() const
{
    Q_D(const TQuickRadiusEffect);

    return d->sourceItem;
}

void TQuickRadiusEffect::setSourceItem(QQuickItem *item)
{
    Q_D(TQuickRadiusEffect);

    if (item == d->sourceItem)
        return;

    if (d->sourceItem) {
        QQuickItemPrivate *sourceItemPrivate = QQuickItemPrivate::get(d->sourceItem);
        sourceItemPrivate->derefFromEffectItem(d->hideSource);
        disconnect(d->sourceItem, SIGNAL(destroyed(QObject*)), this, SLOT(sourceItemDestroyed(QObject*)));

        if (window())
            sourceItemPrivate->derefWindow();
    }

    d->sourceItem = item;

    if (d->sourceItem) {
        if (window() == d->sourceItem->window()
            || (window() == nullptr && d->sourceItem->window())
            || (d->sourceItem->window() == nullptr && window())) {
            QQuickItemPrivate *sourceItemPrivate = QQuickItemPrivate::get(d->sourceItem);
            if (window())
                sourceItemPrivate->refWindow(window());
            else if (d->sourceItem->window())
                sourceItemPrivate->refWindow(d->sourceItem->window());
            sourceItemPrivate->refFromEffectItem(d->hideSource);
            connect(d->sourceItem, SIGNAL(destroyed(QObject*)), this, SLOT(sourceItemDestroyed(QObject*)));
        } else {
            qWarning("TRadiusEffect: sourceItem and TRadiusEffect must both be children of the same window.");
            d->sourceItem = nullptr;
        }
    }
    update();
    emit sourceItemChanged();
}

void TQuickRadiusEffect::sourceItemDestroyed(QObject *item)
{
    Q_D(TQuickRadiusEffect);

    Q_ASSERT(item == d->sourceItem);
    Q_UNUSED(item);
    d->sourceItem = nullptr;
    update();
    emit sourceItemChanged();
}

bool TQuickRadiusEffect::hideSource() const
{
    Q_D(const TQuickRadiusEffect);

    return d->hideSource;
}

void TQuickRadiusEffect::setHideSource(bool hide)
{
    Q_D(TQuickRadiusEffect);

    if (hide == d->hideSource)
        return;

    if (d->sourceItem) {
        QQuickItemPrivate::get(d->sourceItem)->refFromEffectItem(hide);
        QQuickItemPrivate::get(d->sourceItem)->derefFromEffectItem(d->hideSource);
    }
    d->hideSource = hide;
    emit hideSourceChanged();
}

QSGNode *TQuickRadiusEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    Q_D(TQuickRadiusEffect);

    if (Q_UNLIKELY(!d->sourceItem || d->sourceItem->width() <= 0 || d->sourceItem->height() <= 0)) {
        delete oldNode;
        return nullptr;
    }

    if (Q_UNLIKELY(width() <= 0 || height() <= 0)) {
        delete oldNode;
        return nullptr;
    }

    if (Q_UNLIKELY(!d->sourceItem->textureProvider())) {
        delete oldNode;
        return nullptr;
    }

    auto sgRendererInterface = d->window->rendererInterface();
    if (sgRendererInterface && sgRendererInterface->graphicsApi() == QSGRendererInterface::Software) {
        // TODO: Software is not currently supported
        return nullptr;
    } else {
        TSGRadiusImageNode *node = static_cast<TSGRadiusImageNode *>(oldNode);
        if (Q_LIKELY(!node)) {
            node = new TSGRadiusImageNode;
        }
        node->setTextureProvider(d->sourceItem->textureProvider());
        node->setRect(boundingRect());
        node->setRadius(d->radius);
        if (Q_LIKELY(d->extraRadius.isAllocated())) {
            node->setTopLeftRadius(d->extraRadius.value().topLeftRadius);
            node->setTopRightRadius(d->extraRadius.value().topRightRadius);
            node->setBottomLeftRadius(d->extraRadius.value().bottomLeftRadius);
            node->setBottomRightRadius(d->extraRadius.value().bottomRightRadius);
        } else {
            node->setTopLeftRadius(-1.);
            node->setTopRightRadius(-1.);
            node->setBottomLeftRadius(-1.);
            node->setBottomRightRadius(-1.);
        }
        node->setAntialiasing(antialiasing());

        return node;
    }
}

void TQuickRadiusEffect::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(TQuickRadiusEffect);

    if (change == QQuickItem::ItemSceneChange && d->sourceItem) {
        if (value.window)
            QQuickItemPrivate::get(d->sourceItem)->refWindow(value.window);
        else
            QQuickItemPrivate::get(d->sourceItem)->derefWindow();
    }

    QQuickItem::itemChange(change, value);
}

TQuickRadiusEffect::TQuickRadiusEffect(TQuickRadiusEffectPrivate &dd, QQuickItem *parent)
    : QQuickItem(dd, parent)
{
}

#include "tquickradiuseffect.moc"
