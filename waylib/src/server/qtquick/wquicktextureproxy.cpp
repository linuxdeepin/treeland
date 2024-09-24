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
        old->disconnect(q);
        QQuickItemPrivate *sd = QQuickItemPrivate::get(old);
        sd->derefFromEffectItem(hideSource);

        if (old->property(LAYER).toBool()) {
            sd->layer()->setEnabled(false);
            old->setProperty(LAYER, QVariant());
        }
    }

    if (item) {
        QQuickItemPrivate *sd = QQuickItemPrivate::get(item);
        sd->refFromEffectItem(hideSource);

        if (!item->isTextureProvider()) {
            item->setProperty(LAYER, true);
            sd->layer()->setEnabled(true);
        }

        QObject::connect(item, &QQuickItem::destroyed, q, &WQuickTextureProxy::update);
    }

    updateImplicitSize();
}

void WQuickTextureProxyPrivate::updateImplicitSize()
{
    if (sourceItem) {
        W_Q(WQuickTextureProxy);
        q->setImplicitSize(sourceItem->implicitWidth(), sourceItem->implicitHeight());
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

class Q_DECL_HIDDEN SGTextureProviderNode : public QObject, public QSGNode
{
    Q_OBJECT
public:
    SGTextureProviderNode()
        : m_tp(nullptr)
        , m_image(nullptr)
    {

    }

    void setTextureProvider(QSGTextureProvider *tp) {
        if (m_tp == tp)
            return;

        if (m_tp) {
            disconnect(m_tp, &QSGTextureProvider::textureChanged,
                       this, &SGTextureProviderNode::onTextureChanged);
        }

        m_tp = tp;
        onTextureChanged();
        connect(tp, &QSGTextureProvider::textureChanged,
                this, &SGTextureProviderNode::onTextureChanged, Qt::DirectConnection);
    }

    inline QSGTextureProvider *textureProvider() const {
        return m_tp;
    }

    void setImageNode(QSGImageNode *image) {
        m_image = image;
        image->setOwnsTexture(false);
        appendChildNode(image);
        image->setFlag(QSGNode::OwnedByParent);
        if (m_tp) {
            m_image->setTexture(m_tp->texture());
            Q_ASSERT(m_image->texture());
        }
    }

    inline QSGImageNode *image() const {
        return m_image;
    }

private:
    void onTextureChanged() {
        if (!m_image)
            return;

        auto texture = m_tp ? m_tp->texture() : nullptr;

        if (texture) {
            m_image->setTexture(m_tp->texture());
        } else {
            delete m_image;
            m_image = nullptr;
        }
    }

    QPointer<QSGTextureProvider> m_tp;
    QSGImageNode *m_image;
};

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

    auto node = static_cast<SGTextureProviderNode*>(old);
    if (Q_UNLIKELY(!node)) {
        node = new SGTextureProviderNode();
        node->setImageNode(window()->createImageNode());
    } else if (Q_UNLIKELY(!node->image())) {
        node->setImageNode(window()->createImageNode());
    }

    node->setTextureProvider(tp);
    QSGImageNode *imageNode = node->image();
    Q_ASSERT(imageNode);

    QRectF sourceRect = d->sourceRect;
    if (!sourceRect.isValid())
        sourceRect = QRectF(QPointF(0, 0), tp->texture()->textureSize());

    imageNode->setSourceRect(sourceRect);
    imageNode->setRect(QRectF(QPointF(0, 0), size()));
    imageNode->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    imageNode->setMipmapFiltering(d->mipmap ? imageNode->filtering() : QSGTexture::None);
    imageNode->setAnisotropyLevel(antialiasing() ? QSGTexture::Anisotropy4x : QSGTexture::AnisotropyNone);

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

#include "wquicktextureproxy.moc"
