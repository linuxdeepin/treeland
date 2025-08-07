// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderbufferblitter.h"
#include "wrenderbuffernode_p.h"
#include "private/wglobal_p.h"

#include <QSGImageNode>
#include <QSGTextureProvider>
#include <private/qsgplaintexture_p.h>
#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN BlitTextureProvider : public QSGTextureProvider {
public:
    BlitTextureProvider()
        : QSGTextureProvider()
    {

    }

    inline QSGTexture *texture() const override {
        return m_texture;
    }
    inline void setTexture(QSGTexture *tex) {
        m_texture = tex;
    }

private:
    QSGTexture *m_texture = nullptr;
};

class Content;
class Q_DECL_HIDDEN WRenderBufferBlitterPrivate : public WObjectPrivate
{
public:
    WRenderBufferBlitterPrivate(WRenderBufferBlitter *qq)
        : WObjectPrivate(qq)
    {

    }

    ~WRenderBufferBlitterPrivate() {
        cleanTextureProvider();
    }

    static inline WRenderBufferBlitterPrivate *get(WRenderBufferBlitter *qq) {
        return qq->d_func();
    }

    void init();

    inline QQmlListProperty<QObject> data() {
        if (!container)
            return QQuickItemPrivate::get(q_func())->data();

        return QQuickItemPrivate::get(container)->data();
    }

    BlitTextureProvider *ensureTextureProvider() const;
    void cleanTextureProvider();

    W_DECLARE_PUBLIC(WRenderBufferBlitter)
    Content *content;
    QQuickItem *container = nullptr;
    mutable BlitTextureProvider *tp = nullptr;
};

class Q_DECL_HIDDEN Content : public QQuickItem
{
public:
    explicit Content(WRenderBufferBlitter *parent)
        : QQuickItem(parent) {

    }

    inline WRenderBufferBlitterPrivate *d() const {
        auto p = qobject_cast<WRenderBufferBlitter*>(parent());
        Q_ASSERT(p);
        return WRenderBufferBlitterPrivate::get(p);
    }

    bool isTextureProvider() const override {
        return true;
    }

    QSGTextureProvider *textureProvider() const override {
        if (QQuickItem::isTextureProvider())
            return QQuickItem::textureProvider();

        return d()->ensureTextureProvider();
    }

    inline bool offscreen() const {
        return !flags().testFlag(ItemHasContents);
    }

    inline bool setOffscreen(bool newOffscreen) {
        if (offscreen() == newOffscreen)
            return false;

        if (d()->tp) {
            if (newOffscreen)
                disconnect(d()->tp, &BlitTextureProvider::textureChanged, this, &Content::update);
            else
                connect(d()->tp, &BlitTextureProvider::textureChanged, this, &Content::update);
        }

        setFlag(ItemHasContents, !newOffscreen);
        return true;
    }

private:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override {
        const auto tp = d()->ensureTextureProvider();
        if (Q_LIKELY(!tp->texture())) {
            delete old;
            return nullptr;
        }

        auto node = static_cast<QSGImageNode*>(old);
        if (Q_UNLIKELY(!node)) {
            node = window()->createImageNode();
            node->setOwnsTexture(false);
        }

        auto texture = tp->texture();
        node->setTexture(texture);

        const QRectF sourceRect(QPointF(0, 0), texture->textureSize());

        node->setSourceRect(sourceRect);
        node->setRect(QRectF(QPointF(0, 0), size()));
        node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
        node->setAnisotropyLevel(antialiasing() ? QSGTexture::Anisotropy4x : QSGTexture::AnisotropyNone);

        return node;
    }
};

void WRenderBufferBlitterPrivate::init()
{
    W_Q(WRenderBufferBlitter);
    content = new Content(q);

    if (q->window()->graphicsApi() != QSGRendererInterface::Software) {
        container = new QQuickItem(q);

        auto d = QQuickItemPrivate::get(container);
        d->refFromEffectItem(true);
    }
}

BlitTextureProvider *WRenderBufferBlitterPrivate::ensureTextureProvider() const
{
    if (Q_LIKELY(tp))
        return tp;

    tp = new BlitTextureProvider();

    if (!content->offscreen())
        tp->connect(tp, &BlitTextureProvider::textureChanged, content, &Content::update);

    return tp;
}

void WRenderBufferBlitterPrivate::cleanTextureProvider()
{
    if (tp) {
        QQuickWindowQObjectCleanupJob::schedule(q_func()->window(), tp);
        tp = nullptr;
    }
}

WRenderBufferBlitter::WRenderBufferBlitter(QQuickItem *parent)
    : QQuickItem(parent)
    , WObject(*new WRenderBufferBlitterPrivate(this))
{
    setFlag(ItemHasContents);
    W_D(WRenderBufferBlitter);
    d->init();
}

WRenderBufferBlitter::~WRenderBufferBlitter()
{

}

QQuickItem *WRenderBufferBlitter::content() const
{
    W_DC(WRenderBufferBlitter);
    return d->content;
}

bool WRenderBufferBlitter::offscreen() const
{
    W_DC(WRenderBufferBlitter);
    return d->content->offscreen();
}

void WRenderBufferBlitter::setOffscreen(bool newOffscreen)
{
    W_D(WRenderBufferBlitter);
    if (d->content->setOffscreen(newOffscreen))
        Q_EMIT offscreenChanged();
}

void WRenderBufferBlitter::invalidateSceneGraph()
{
    W_D(WRenderBufferBlitter);
    delete d->tp;
    d->tp = nullptr;
}

static void onTextureChanged(WRenderBufferNode *node, void *data) {
    auto *d = reinterpret_cast<WRenderBufferBlitterPrivate*>(data);
    if (!d->tp)
        return;

    d->tp->setTexture(node->texture());
    Q_EMIT d->tp->textureChanged();
}

QSGNode *WRenderBufferBlitter::updatePaintNode(QSGNode *oldNode, [[maybe_unused]] QQuickItem::UpdatePaintNodeData *oldData)
{

    auto node = static_cast<WRenderBufferNode*>(oldNode);
    if (Q_LIKELY(node)) {
        node->resize(size());
        return node;
    }

    W_D(WRenderBufferBlitter);
    if (window()->graphicsApi() == QSGRendererInterface::Software) {
        node = WRenderBufferNode::createSoftwareNode(this);
    } else {
        node = WRenderBufferNode::createRhiNode(this);
    }

    node->setContentItem(d->container);
    node->setTextureChangedCallback(onTextureChanged, d);
    node->resize(size());
    onTextureChanged(node, d);

    return node;
}

void WRenderBufferBlitter::itemChange(ItemChange type, const ItemChangeData &data)
{
    if (type == ItemDevicePixelRatioHasChanged) {
        update();
    }

    QQuickItem::itemChange(type, data);
}

void WRenderBufferBlitter::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    W_D(WRenderBufferBlitter);
    d->content->setSize(newGeometry.size());

    if (d->container)
        d->container->setSize(newGeometry.size());
}

void WRenderBufferBlitter::releaseResources()
{
    W_D(WRenderBufferBlitter);
    d->cleanTextureProvider();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wrenderbufferblitter.cpp"
