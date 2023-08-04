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
    return item->mapToGlobal(item->contentItem()->position());
}

QPointF SurfaceHandler::mapFromGlobal(const QPointF &pos)
{
    return item->contentItem()->mapFromGlobal(pos);
}

QPointF SurfaceHandler::mapToGlobal(const QPointF &pos)
{
    return item->contentItem()->mapToGlobal(pos);
}

class WSurfaceItemPrivate : public QQuickItemPrivate
{
    friend class WSGTextureProvider;
public:
    WSurfaceItemPrivate();
    ~WSurfaceItemPrivate();

    inline static WSurfaceItemPrivate *get(WSurfaceItem *qq) {
        return qq->d_func();
    }

    void onSurfaceCommit();
    void setImplicitPosition(QPointF newImplicitPosition);

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
    QQuickItem *contentItem = nullptr;
    QQuickItem *eventItem = nullptr;
    WSurfaceItem::ResizeMode resizeMode = WSurfaceItem::SizeFromSurface;
    // For xdg popup surface
    // TODO: split the implementations of xdg surface to WXdgSurfaceItem,
    // and split out the related implementations from WSurface too.
    QPointF implicitPosition;

    QMetaObject::Connection frameDoneConnection;
    std::unique_ptr<SurfaceHandler> surfaceHandler;
    WSGTextureProvider *textureProvider = nullptr;
};

class ContentItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit ContentItem(WSurfaceItem *parent);

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
    void releaseResources() override;

    // Using by Qt library
    Q_PRIVATE_SLOT(d(), void invalidateSceneGraph())
};

class EventItem : public QQuickItem
{
public:
    explicit EventItem(ContentItem *parent)
        : QQuickItem(parent) {
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::AllButtons);
        setFlag(QQuickItem::ItemClipsChildrenToShape, true);
    }

    inline WSurfaceItemPrivate *d() const {
        return WSurfaceItemPrivate::get(static_cast<ContentItem*>(parent())->surfaceItem());
    }

    bool contains(const QPointF &point) const override {
        return d()->contains(point);
    }

private:
    bool event(QEvent *event) override {
        if (d()->event(event))
            return true;
        return QQuickItem::event(event);
    }
};

ContentItem::ContentItem(WSurfaceItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

bool ContentItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *ContentItem::textureProvider() const
{
    return d()->textureProvider;
}

QSGNode *ContentItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    return d()->updatePaintNode(oldNode, data);
}

void ContentItem::releaseResources()
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
    setFlag(ItemIsFocusScope);
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
    auto contentItem = new ContentItem(this);
    d->contentItem = contentItem;
    d->eventItem = new EventItem(contentItem);
    QQuickItemPrivate::get(d->eventItem)->anchors()->setFill(d->contentItem);
    if (d->focus)
        d->eventItem->forceActiveFocus();
    connect(this, &WSurfaceItem::focusChanged, d->eventItem, qOverload<bool>(&QQuickItem::setFocus));

    d->initForSurface();
    d->update();

    Q_EMIT surfaceChanged();
}

QQuickItem *WSurfaceItem::contentItem() const
{
    Q_D(const WSurfaceItem);
    return d->contentItem;
}

WSurfaceItem::ResizeMode WSurfaceItem::resizeMode() const
{
    Q_D(const WSurfaceItem);

    return d->resizeMode;
}

void WSurfaceItem::setResizeMode(ResizeMode newResizeMode)
{
    Q_D(WSurfaceItem);

    if (d->resizeMode == newResizeMode)
        return;
    d->resizeMode = newResizeMode;
    Q_EMIT resizeModeChanged();
}

QPointF WSurfaceItem::implicitPosition() const
{
    Q_D(const WSurfaceItem);

    return d->implicitPosition;
}

void WSurfaceItemPrivate::setImplicitPosition(QPointF newImplicitPosition)
{
    Q_Q(WSurfaceItem);

    if (implicitPosition == newImplicitPosition)
        return;
    implicitPosition = newImplicitPosition;
    Q_EMIT q->implicitPositionChanged();
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

    if (d->resizeMode == SizeToSurface && newGeometry.size() != oldGeometry.size()) {
        d->contentItem->setSize(d->contentItem->size() + newGeometry.size() - oldGeometry.size());
        d->surface->resize(d->contentItem->size().toSize());
    }
}

void WSurfaceItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    Q_D(WSurfaceItem);

    if (change == ItemVisibleHasChanged) {
        d->updateFrameDoneConnection();
    }
}

void WSurfaceItem::focusInEvent(QFocusEvent *event)
{
    QQuickItem::focusInEvent(event);

    Q_D(WSurfaceItem);
    if (d->eventItem)
        d->eventItem->forceActiveFocus(event->reason());
}

WSurfaceItemPrivate::WSurfaceItemPrivate()
    : textureProvider(new WSGTextureProvider(this))
{

}

WSurfaceItemPrivate::~WSurfaceItemPrivate()
{
    if (textureProvider)
        delete textureProvider;
    if (surface)
        static_cast<SurfaceHandler*>(surface->handler())->item = nullptr;
}

void WSurfaceItemPrivate::onSurfaceCommit()
{
    Q_Q(WSurfaceItem);

    const QRectF geometry = surface->getContentGeometry();
    if (resizeMode == WSurfaceItem::SizeFromSurface) {
        if (!qFuzzyCompare(implicitWidth, geometry.width()))
            q->setImplicitWidth(geometry.width());
        if (!qFuzzyCompare(implicitHeight, geometry.height()))
            q->setImplicitHeight(geometry.height());

        contentItem->setSize(surface->size());
    }
    contentItem->setPosition(-geometry.topLeft());

    updateSubsurfaceItem();

    QPointF implicitPosition = surface->position() - contentItem->position();
    if (auto parentSurface = surface->parentSurface())
        implicitPosition -= parentSurface->getContentGeometry().topLeft();
    setImplicitPosition(implicitPosition);
}

bool WSurfaceItemPrivate::contains(const QPointF &localPos) const
{
    if (Q_UNLIKELY(!surface))
        return false;

    return surface->inputRegionContains(localPos);
}

QSGNode *WSurfaceItemPrivate::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if (!surface || contentItem->width() <= 0 || contentItem->height() <= 0) {
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
    const QRectF targetGeometry(surface->textureOffset(), contentItem->size());
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
    auto d = QQuickItemPrivate::get(contentItem);

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
        d->window->scheduleRenderJob(new WSurfaceCleanupJob(textureProvider),
                                     QQuickWindow::AfterRenderingStage);
        textureProvider = nullptr;
    }

    if (frameDoneConnection)
        QObject::disconnect(frameDoneConnection);
    surface = nullptr;
    // Force to update the contents, avoid to render the invalid textures
    d->dirty(QQuickItemPrivate::Content);
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
    return contentItem->update();
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
    QObject::connect(surface, &WSurface::primaryOutputChanged, q, [this] {
        updateFrameDoneConnection();
    });
    QObject::connect(surface, SIGNAL(hasSubsurfaceChanged()), q, SLOT(onHasSubsurfaceChanged()));
    QObject::connect(surface->handle(), SIGNAL(commit()), q, SLOT(onSurfaceCommit()));

    onHasSubsurfaceChanged();
    updateFrameDoneConnection();
    onSurfaceCommit();

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
    auto qwsurface = surface->handle();
    Q_ASSERT(qwsurface);
    if (surface->hasSubsurface())
        updateSubsurfaceItem();
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
    Q_ASSERT(contentItem);

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
        item->setPosition(contentItem->position() + QPointF(subsurface->current.x, subsurface->current.y));
    }

    if (prev)
        contentItem->stackAfter(prev);
    prev = contentItem;

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        WSurfaceItem *item = getItemFrom(subsurface);
        if (!item)
            continue;

        item->setParentItem(q);
        Q_ASSERT(item->parentItem() == q);
        Q_ASSERT(prev->parentItem() == item->parentItem());
        item->stackAfter(prev);
        prev = item;
        item->setPosition(contentItem->position() + QPointF(subsurface->current.x, subsurface->current.y));
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfaceitem.cpp"
#include "wsurfaceitem.moc"
