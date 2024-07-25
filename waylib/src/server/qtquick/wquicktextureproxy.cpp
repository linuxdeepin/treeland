// SPDX-FileCopyrightText: 2020 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicktextureproxy.h"
#include "wquicktextureproxy_p.h"

#include <QSGImageNode>
#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WQuickTextureProxyPrivate::~WQuickTextureProxyPrivate()
{
    initSourceItem(sourceItem, nullptr);
}

#define LAYER "__layer_enabled_by_WQuickTextureProxy"

void WQuickTextureProxyPrivate::initSourceItem(QQuickItem *old, QQuickItem *item)
{
    W_Q(WQuickTextureProxy);

    if (old) {
        if (auto tp = old->textureProvider()) {
            bool ok = tp->disconnect(q);
            Q_ASSERT(ok);
        }

        old->disconnect(q);
        QQuickItemPrivate *sd = QQuickItemPrivate::get(old);
        sd->derefFromEffectItem(hideSource);

        if (old->property(LAYER).toBool()) {
            sd->layer()->setEnabled(false);
            old->setProperty(LAYER, QVariant());
        }
    }

    textureSize = QSize();

    if (item) {
        QQuickItemPrivate *sd = QQuickItemPrivate::get(item);
        sd->refFromEffectItem(hideSource);

        if (!item->isTextureProvider()) {
            item->setProperty(LAYER, true);
            sd->layer()->setEnabled(true);
        }

        auto tp = item->textureProvider();
        QObject::connect(tp, &QSGTextureProvider::destroyed, q, &WQuickTextureProxy::update);
        QObject::connect(tp, SIGNAL(textureChanged()), q, SLOT(onTextureChanged()));
        QObject::connect(item, &QQuickItem::destroyed, q, &WQuickTextureProxy::update);

        if (auto texture = tp->texture())
            textureSize = texture->textureSize();
    }

    updateImplicitSize();
}

void WQuickTextureProxyPrivate::onTextureChanged()
{
    W_Q(WQuickTextureProxy);

    const auto texture = sourceItem->textureProvider()->texture();
    QSize newTextureSize = texture ? texture->textureSize() : QSize();
    if (textureSize != newTextureSize) {
        textureSize = newTextureSize;
        updateImplicitSize();
    }
    q->update();
}

void WQuickTextureProxyPrivate::updateImplicitSize()
{
    if (!textureSize.isEmpty()) {
        W_Q(WQuickTextureProxy);
        const qreal dpr = q->window() ? q->window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio();
        q->setImplicitSize(textureSize.width() / dpr, textureSize.height() / dpr);
    }
}

WQuickTextureProxy::WQuickTextureProxy(QQuickItem *parent)
    : QQuickItem (parent)
    , WObject(*new WQuickTextureProxyPrivate(this))
{
    setFlag(ItemHasContents);
}

WQuickTextureProxy::~WQuickTextureProxy()
{

}

QQuickItem *WQuickTextureProxy::sourceItem() const
{
    W_DC(WQuickTextureProxy);
    return d->sourceItem;
}

QRectF WQuickTextureProxy::sourceRect() const
{
    W_DC(WQuickTextureProxy);
    return d->sourceRect;
}

bool WQuickTextureProxy::hideSource() const
{
    W_DC(WQuickTextureProxy);
    return d->hideSource;
}

void WQuickTextureProxy::setHideSource(bool newHideSource)
{
    W_D(WQuickTextureProxy);
    if (d->hideSource == newHideSource)
        return;

    if (d->sourceItem) {
        QQuickItemPrivate::get(d->sourceItem)->refFromEffectItem(newHideSource);
        QQuickItemPrivate::get(d->sourceItem)->derefFromEffectItem(d->hideSource);
    }
    d->hideSource = newHideSource;
    Q_EMIT hideSourceChanged();
}

bool WQuickTextureProxy::mipmap() const
{
    W_DC(WQuickTextureProxy);
    return d->mipmap;
}

void WQuickTextureProxy::setMipmap(bool newMipmap)
{
    W_D(WQuickTextureProxy);
    if (d->mipmap == newMipmap)
        return;
    d->mipmap = newMipmap;
    update();
    Q_EMIT mipmapChanged();
}

bool WQuickTextureProxy::isTextureProvider() const
{
    if (QQuickItem::isTextureProvider())
        return true;

    W_DC(WQuickTextureProxy);
    return d->sourceItem && d->sourceItem->isTextureProvider();
}

QSGTextureProvider *WQuickTextureProxy::textureProvider() const
{
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    W_DC(WQuickTextureProxy);
    if (!d->sourceItem)
        return nullptr;

    return d->sourceItem->textureProvider();
}

void WQuickTextureProxy::setSourceItem(QQuickItem *sourceItem)
{
    W_D(WQuickTextureProxy);

    if (d->sourceItem == sourceItem)
        return;

    if (isComponentComplete()) {
        d->initSourceItem(d->sourceItem, sourceItem);
    }

    d->sourceItem = sourceItem;
    Q_EMIT sourceItemChanged();
    update();
}

void WQuickTextureProxy::setSourceRect(const QRectF &sourceRect)
{
    W_D(WQuickTextureProxy);
    if (d->sourceRect == sourceRect)
        return;

    d->sourceRect = sourceRect;
    Q_EMIT sourceRectChanged();
    update();
}

QSGNode *WQuickTextureProxy::updatePaintNode(QSGNode *old, QQuickItem::UpdatePaintNodeData *)
{
    W_D(WQuickTextureProxy);

    if (Q_UNLIKELY(!d->sourceItem)) {
        delete old;
        return nullptr;
    }

    const auto tp = d->sourceItem->textureProvider();
    if (Q_LIKELY(!tp || !tp->texture())) {
        delete old;
        return nullptr;
    }

    auto node = static_cast<QSGImageNode*>(old);
    if (Q_UNLIKELY(!node)) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
    }

    auto texture = tp->texture();
    Q_ASSERT(d->textureSize == texture->textureSize());
    node->setTexture(texture);

    QRectF sourceRect = d->sourceRect;
    if (!sourceRect.isValid())
        sourceRect = QRectF(QPointF(0, 0), texture->textureSize());

    node->setSourceRect(sourceRect);
    node->setRect(QRectF(QPointF(0, 0), size()));
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    node->setMipmapFiltering(d->mipmap ? node->filtering() : QSGTexture::None);
    node->setAnisotropyLevel(antialiasing() ? QSGTexture::Anisotropy4x : QSGTexture::AnisotropyNone);

    return node;
}

void WQuickTextureProxy::componentComplete()
{
    W_D(WQuickTextureProxy);

    if (d->sourceItem)
        d->initSourceItem(nullptr, d->sourceItem);

    return QQuickItem::componentComplete();
}

void WQuickTextureProxy::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange)
        d_func()->updateImplicitSize();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquicktextureproxy.cpp"
