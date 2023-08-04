// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurfaceitem.h"
#include "wsurface.h"
#include "wsurfacehandler.h"
#include "wtexture.h"
#include "wseat.h"
#include "wcursor.h"
#include "woutput.h"
#include "woutputviewport.h"

#include <qwcompositor.h>
#include <qwsubcompositor.h>

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <private/qquickitem_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGTextureProvider : public QSGTextureProvider
{
public:
    WSGTextureProvider(WSurfaceItemPrivate *item);

public:
    QSGTexture *texture() const override;
    void updateTexture(); // in render thread

private:
    WSurfaceItemPrivate *item;
    QScopedPointer<WTexture> dwtexture;
};

class SurfaceHandler : public WSurfaceHandler
{
public:
    SurfaceHandler(WSurfaceItem *item)
        : WSurfaceHandler(item->surface())
        , item(item)
    {}

    QPointF position() const override;
    QPointF mapFromGlobal(const QPointF &pos) override;
    QPointF mapToGlobal(const QPointF &pos) override;

    WSurfaceItem *item;
};

QPointF SurfaceHandler::position() const
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

QPointF SurfaceHandler::mapFromGlobal(const QPointF &pos)
{
    return item->mapFromGlobal(pos);
}

QPointF SurfaceHandler::mapToGlobal(const QPointF &pos)
{
    return item->mapToGlobal(pos);
}

class WSurfaceItemPrivate : public QQuickItemPrivate
{
    friend class WSGTextureProvider;
public:
    WSurfaceItemPrivate()
        : textureProvider(new WSGTextureProvider(this))
    {

    }
    ~WSurfaceItemPrivate() {
        if (textureProvider)
            delete textureProvider;
        if (surface)
            static_cast<SurfaceHandler*>(surface->handler())->item = nullptr;
    }

    inline static WSurfaceItemPrivate *get(WSurfaceItem *qq) {
        return qq->d_func();
    }

    qreal getImplicitWidth() const override {
        return surface ? surface->size().width() : 0;
    }
    qreal getImplicitHeight() const override {
        return surface ? surface->size().height() : 0;
    }

    void updateImplicitSize() {
        implicitWidthChanged();
        implicitHeightChanged();

        Q_Q(WSurfaceItem);
        if (!widthValid())
            q->resetWidth();
        if (!heightValid())
            q->resetHeight();
    }

    // For QQuickItem
    bool contains(const QPointF &localPos) const;
    QSGNode *updatePaintNode(QSGNode *, QQuickItem::UpdatePaintNodeData *);
    bool event(QEvent *event);
    void releaseResources();
    void invalidateSceneGraph();
    void update();
    // End

    void initForSurface();
    void updateFrameDoneConnection();

    void onHasSubsurfaceChanged();
    void updateSubsurfaceItem();

    Q_DECLARE_PUBLIC(WSurfaceItem)
    QPointer<WSurface> surface;
    QQuickItem *proxyItem = nullptr;
    QMetaObject::Connection frameDoneConnection;
    std::unique_ptr<SurfaceHandler> surfaceHandler;
    WSGTextureProvider *textureProvider = nullptr;
};

static void setItemForSurface(QQuickItem *item, bool on)
{
    item->setFlag(QQuickItem::ItemHasContents, on);
    item->setAcceptHoverEvents(on);
    item->setAcceptedMouseButtons(on ? Qt::AllButtons : Qt::NoButton);
}

class ProxyItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit ProxyItem(WSurfaceItem *parent);

    inline WSurfaceItem *surfaceItem() const {
        return qobject_cast<WSurfaceItem*>(parent());
    }
    inline WSurfaceItemPrivate *d() const {
        return WSurfaceItemPrivate::get(surfaceItem());
    }

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

private:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    bool event(QEvent *event) override;
    void releaseResources() override;

    // Using by Qt library
    Q_PRIVATE_SLOT(d(), void invalidateSceneGraph())
};

ProxyItem::ProxyItem(WSurfaceItem *parent)
    : QQuickItem(parent)
{
    QQuickItemPrivate::get(this)->anchors()->setFill(parent);
    setItemForSurface(this, true);
}

bool ProxyItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *ProxyItem::textureProvider() const
{
    return d()->textureProvider;
}

QSGNode *ProxyItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    return d()->updatePaintNode(oldNode, data);
}

bool ProxyItem::event(QEvent *event)
{
    if (d()->event(event))
        return true;
    return QQuickItem::event(event);
}

void ProxyItem::releaseResources()
{
    return d()->releaseResources();
}

WSGTextureProvider::WSGTextureProvider(WSurfaceItemPrivate *item)
    : item(item)
{
    dwtexture.reset(new WTexture(nullptr));
}

QSGTexture *WSGTextureProvider::texture() const
{
    return dwtexture->getSGTexture(item->window);
}

void WSGTextureProvider::updateTexture()
{
    dwtexture->setHandle(item->surface->texture());
    Q_EMIT textureChanged();
    item->update();
}

WSurfaceItem::WSurfaceItem(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemPrivate(), parent)
{
    setItemForSurface(this, true);
}

bool WSurfaceItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *WSurfaceItem::textureProvider() const
{
    Q_D(const WSurfaceItem);
    return d->textureProvider;
}

bool WSurfaceItem::contains(const QPointF &localPos) const
{
    Q_D(const WSurfaceItem);
    return d->contains(localPos);
}

WSurface *WSurfaceItem::surface() const
{
    Q_D(const WSurfaceItem);
    return d->surface.get();
}

void WSurfaceItem::setSurface(WSurface *surface)
{
    Q_D(WSurfaceItem);
    if (d->surface == surface || (d->componentComplete && d->surface))
        return;

    Q_ASSERT(surface);
    d->surface = surface;
    d->initForSurface();
    d->update();

    Q_EMIT surfaceChanged();
}

QQuickItem *WSurfaceItem::contentItem() const
{
    Q_D(const WSurfaceItem);
    return d->proxyItem ? d->proxyItem : const_cast<WSurfaceItem*>(this);
}

QSGNode *WSurfaceItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *data)
{
    Q_D(WSurfaceItem);
    return d->updatePaintNode(oldNode, data);
}

bool WSurfaceItem::event(QEvent *event)
{
    Q_D(WSurfaceItem);
    if (d->event(event))
        return true;
    return QQuickItem::event(event);
}

void WSurfaceItem::releaseResources()
{
    Q_D(WSurfaceItem);
    d->releaseResources();
    QQuickItem::releaseResources();
}

void WSurfaceItem::componentComplete()
{
    Q_D(WSurfaceItem);

    if (d->surface) {
        d->initForSurface();
    }

    QQuickItem::componentComplete();
}

void WSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(WSurfaceItem);

    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

void WSurfaceItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    Q_D(WSurfaceItem);

    if (change == ItemVisibleHasChanged) {
        d->updateFrameDoneConnection();
    }
}

bool WSurfaceItemPrivate::contains(const QPointF &localPos) const
{
    if (Q_UNLIKELY(!surface))
        return false;

    return surface->inputRegionContains(localPos);
}

QSGNode *WSurfaceItemPrivate::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    Q_Q(WSurfaceItem);

    if (!surface || q->width() <= 0 || q->height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        auto texture = textureProvider->texture();
        if (!texture)
            return node;

        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setTexture(texture);
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry = surface->handle()->getBufferSourceBox();
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(surface->textureOffset(), q->size());
    node->setRect(targetGeometry);
    node->setFiltering(QSGTexture::Linear);

    return node;
}

bool WSurfaceItemPrivate::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::HoverEnter: Q_FALLTHROUGH();
    case QEvent::HoverLeave: Q_FALLTHROUGH();
    case QEvent::MouseButtonPress: Q_FALLTHROUGH();
    case QEvent::MouseButtonRelease: Q_FALLTHROUGH();
    case QEvent::MouseMove: Q_FALLTHROUGH();
    case QEvent::HoverMove: Q_FALLTHROUGH();
    case QEvent::KeyPress: Q_FALLTHROUGH();
    case QEvent::KeyRelease: {
        auto e = static_cast<QInputEvent*>(event);
        return WSeat::sendEvent(surface.get(), e);
    }
    }

    return false;
}

void WSurfaceItemPrivate::releaseResources()
{
    if (textureProvider) {
        class WSurfaceCleanupJob : public QRunnable
        {
        public:
            WSurfaceCleanupJob(QObject *object) : m_object(object) { }
            void run() override {
                delete m_object;
            }
            QObject *m_object;
        };

        // Delay clean the textures on the next render after.
        window->scheduleRenderJob(new WSurfaceCleanupJob(textureProvider),
                                  QQuickWindow::AfterRenderingStage);
        textureProvider = nullptr;
    }

    if (frameDoneConnection)
        QObject::disconnect(frameDoneConnection);
    surface = nullptr;
    // Force to update the contents, avoid to render the invalid textures
    if (proxyItem)
        QQuickItemPrivate::get(proxyItem)->dirty(QQuickItemPrivate::Content);
    else
        dirty(QQuickItemPrivate::Content);
}

void WSurfaceItemPrivate::invalidateSceneGraph()
{
    if (textureProvider)
        delete textureProvider;
    textureProvider = nullptr;
    surface = nullptr;
}

void WSurfaceItemPrivate::update()
{
    return proxyItem ? proxyItem->update() : q_func()->update();
}

static WSurfaceItem *getItemFrom(wlr_surface *s)
{
    WSurface *surface = nullptr;

    if (auto ss = QWSurface::get(s)) {
        surface = WSurface::fromHandle(ss);
        if (!surface || !surface->handler())
            return nullptr;
    }

    auto handler = static_cast<SurfaceHandler*>(surface->handler());
    return handler->item;
}

void WSurfaceItemPrivate::initForSurface()
{
    Q_Q(WSurfaceItem);

    surfaceHandler.reset(new SurfaceHandler(q));
    surface->setHandler(surfaceHandler.get());
    textureProvider->updateTexture();

    QObject::connect(surface, &WSurface::textureChanged,
                     textureProvider, &WSGTextureProvider::updateTexture);
    QObject::connect(surface, &WSurface::destroyed, q,
                     &WSurfaceItem::releaseResources, Qt::DirectConnection);
    QObject::connect(surface, &WSurface::sizeChanged, q, [this] {
        updateImplicitSize();
    });
    QObject::connect(surface, &WSurface::primaryOutputChanged, q, [this] {
        updateFrameDoneConnection();
    });
    QObject::connect(surface, SIGNAL(hasSubsurfaceChanged()), q, SLOT(onHasSubsurfaceChanged()));

    onHasSubsurfaceChanged();
    updateImplicitSize();
    updateFrameDoneConnection();

    if (surface->isSubsurface()) {
        if (auto sub = QWSubsurface::tryFrom(surface->handle()))
            if (auto pItem = getItemFrom(sub->handle()->parent))
                pItem->d_func()->updateSubsurfaceItem();
    }
}

void WSurfaceItemPrivate::updateFrameDoneConnection()
{
    if (frameDoneConnection)
        QObject::disconnect(frameDoneConnection);

    if (!effectiveVisible)
        return;

    if (auto output = surface->primaryOutput()) {
        auto viewport = WOutputViewport::get(output);
        if (!viewport)
            return;
        frameDoneConnection = QObject::connect(viewport, &WOutputViewport::frameDone,
                                               surface, &WSurface::notifyFrameDone);
    }
}

void WSurfaceItemPrivate::onHasSubsurfaceChanged()
{
    Q_Q(WSurfaceItem);

    if (bool(proxyItem) == surface->hasSubsurface())
        return;

    auto qwsurface = surface->handle();
    Q_ASSERT(qwsurface);
    if (surface->hasSubsurface()) {
        Q_ASSERT(!proxyItem);
        proxyItem = new ProxyItem(q);

        QObject::connect(qwsurface, SIGNAL(commit()), q, SLOT(updateSubsurfaceItem()));
        updateSubsurfaceItem();
    } else {
        proxyItem->setVisible(false);
        proxyItem->deleteLater();
        proxyItem = nullptr;

        for (WSurfaceItem* subItem : q->findChildren<WSurfaceItem*>(Qt::FindDirectChildrenOnly))
            subItem->setParentItem(q->parentItem());

        QObject::disconnect(qwsurface, SIGNAL(commit()), q, SLOT(updateSubsurfaceItem()));
    }

    setItemForSurface(q, !proxyItem);
    Q_EMIT q->contentItemChanged();
}

inline static WSurfaceItem *getItemFrom(wlr_subsurface *sub)
{
    return getItemFrom(sub->surface);
}

void WSurfaceItemPrivate::updateSubsurfaceItem()
{
    Q_Q(WSurfaceItem);
    auto surface = this->surface->handle()->handle();
    Q_ASSERT(surface);
    Q_ASSERT(proxyItem);

    QQuickItem *prev = nullptr;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        WSurfaceItem *item = getItemFrom(subsurface);
        if (!item)
            continue;

        item->setParentItem(q);
        Q_ASSERT(item->parentItem() == q);
        if (prev) {
            Q_ASSERT(prev->parentItem() == item->parentItem());
            item->stackAfter(prev);
        }
        prev = item;
        item->setPosition(QPointF(subsurface->current.x, subsurface->current.y));
    }

    if (prev)
        proxyItem->stackBefore(prev);
    prev = proxyItem;

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        WSurfaceItem *item = getItemFrom(subsurface);
        if (!item)
            continue;

        item->setParentItem(q);
        Q_ASSERT(item->parentItem() == q);
        Q_ASSERT(prev->parentItem() == item->parentItem());
        item->stackAfter(prev);
        prev = item;
        item->setPosition(QPointF(subsurface->current.x, subsurface->current.y));
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfaceitem.cpp"
#include "wsurfaceitem.moc"
