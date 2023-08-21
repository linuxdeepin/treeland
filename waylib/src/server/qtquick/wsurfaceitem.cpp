// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurfaceitem.h"
#include "wsurface.h"
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

class ContentItem;
class WSGTextureProvider : public QSGTextureProvider
{
    friend class ContentItem;
public:
    WSGTextureProvider(ContentItem *item);

public:
    QSGTexture *texture() const override;
    void updateTexture(); // in render thread

private:
    ContentItem *item;
    QScopedPointer<WTexture> dwtexture;
};

class WSurfaceItemPrivate : public QQuickItemPrivate
{
    friend class WSGTextureProvider;
public:
    WSurfaceItemPrivate();
    ~WSurfaceItemPrivate();

    inline static WSurfaceItemPrivate *get(WSurfaceItem *qq) {
        return qq->d_func();
    }

    void initForSurface();
    void updateFrameDoneConnection();

    void onHasSubsurfaceChanged();
    void updateSubsurfaceItem();
    WSurfaceItem *ensureSubsurfaceItem(WSurface *subsurfaceSurface);

    Q_DECLARE_PUBLIC(WSurfaceItem)
    QPointer<WSurface> surface;
    ContentItem *contentItem = nullptr;
    QQuickItem *eventItem = nullptr;
    WSurfaceItem::ResizeMode resizeMode = WSurfaceItem::SizeFromSurface;
    QList<WSurfaceItem*> subsurfaces;

    QMetaObject::Connection frameDoneConnection;
};

class ContentItem : public QQuickItem
{
    friend class WSurfaceItemPrivate;
    friend class WSurfaceItem;
    Q_OBJECT
public:
    explicit ContentItem(WSurfaceItem *parent);
    ~ContentItem();

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
    Q_SLOT void invalidateSceneGraph();

    WSGTextureProvider *m_textureProvider = nullptr;
    QMetaObject::Connection m_updateTextureConnection;
};

class EventItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit EventItem(ContentItem *parent)
        : QQuickItem(parent) {
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::AllButtons);
        setFlag(QQuickItem::ItemClipsChildrenToShape, true);
    }

    inline bool isValid() const {
        return parent() && static_cast<ContentItem*>(parent())->surfaceItem();
    }

    inline WSurfaceItemPrivate *d() const {
        return WSurfaceItemPrivate::get(static_cast<ContentItem*>(parent())->surfaceItem());
    }

    bool contains(const QPointF &point) const override {
        if (Q_UNLIKELY(!d() || !d()->surface || !d()->surface->handle()))
            return false;

        return d()->surface->inputRegionContains(point);
    }

private:
    bool event(QEvent *event) override {
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
            if (static_cast<ContentItem*>(parent())->surfaceItem()->sendEvent(e))
                return true;
            break;
        }
        case QEvent::FocusOut: {
            if (isValid())
                d()->q_func()->setFocus(false);
            break;
        }
        }

        return QQuickItem::event(event);
    }
};

ContentItem::ContentItem(WSurfaceItem *parent)
    : QQuickItem(parent)
    , m_textureProvider(new WSGTextureProvider(this))
{
    setFlag(QQuickItem::ItemHasContents, true);
}

ContentItem::~ContentItem()
{
    if (m_updateTextureConnection)
        QObject::disconnect(m_updateTextureConnection);

    if (m_textureProvider) {
        delete m_textureProvider;
        m_textureProvider = nullptr;
    }
}

bool ContentItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *ContentItem::textureProvider() const
{
    return m_textureProvider;
}

QSGNode *ContentItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!d()->surface || !m_textureProvider || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        auto texture = m_textureProvider->texture();
        if (!texture)
            return node;

        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setTexture(texture);
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry = d()->surface->handle()->getBufferSourceBox();
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(d()->surface->textureOffset(), size());
    node->setRect(targetGeometry);
    node->setFiltering(QSGTexture::Linear);

    return node;
}

void ContentItem::releaseResources()
{
    auto d = QQuickItemPrivate::get(this);

    if (m_textureProvider) {
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
        d->window->scheduleRenderJob(new WSurfaceCleanupJob(m_textureProvider),
                                     QQuickWindow::AfterRenderingStage);
        m_textureProvider->item = nullptr;
        if (m_updateTextureConnection)
            QObject::disconnect(m_updateTextureConnection);
        m_textureProvider = nullptr;
    }

    // Force to update the contents, avoid to render the invalid textures
    d->dirty(QQuickItemPrivate::Content);
}

void ContentItem::invalidateSceneGraph()
{
    if (m_textureProvider)
        delete m_textureProvider;
    m_textureProvider = nullptr;
}

WSGTextureProvider::WSGTextureProvider(ContentItem *item)
    : item(item)
{
    dwtexture.reset(new WTexture(nullptr));
}

QSGTexture *WSGTextureProvider::texture() const
{
    return dwtexture->getSGTexture(item->window());
}

void WSGTextureProvider::updateTexture()
{
    dwtexture->setHandle(item->d()->surface->texture());
    Q_EMIT textureChanged();
    item->update();
}

WSurfaceItem::WSurfaceItem(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemPrivate(), parent)
{
    setFlag(ItemIsFocusScope);

    Q_D(WSurfaceItem);
    auto contentItem = new ContentItem(this);
    d->contentItem = contentItem;
    d->eventItem = new EventItem(contentItem);
    QQuickItemPrivate::get(d->eventItem)->anchors()->setFill(d->contentItem);
}

WSurfaceItem::~WSurfaceItem()
{

}

WSurfaceItem *WSurfaceItem::fromFocusObject(QObject *focusObject)
{
    if (auto item = qobject_cast<EventItem*>(focusObject))
        return item->d()->q_func();
    return nullptr;
}

bool WSurfaceItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *WSurfaceItem::textureProvider() const
{
    Q_D(const WSurfaceItem);
    return d->contentItem->textureProvider();
}

WSurface *WSurfaceItem::surface() const
{
    Q_D(const WSurfaceItem);
    return d->surface.get();
}

void WSurfaceItem::setSurface(WSurface *surface)
{
    Q_D(WSurfaceItem);
    if (d->surface == surface)
        return;

    auto oldSurface = d->surface;
    d->surface = surface;
    if (d->componentComplete) {
        if (oldSurface) {
            oldSurface->disconnect(this);
            oldSurface->disconnect(d->contentItem->m_textureProvider);
        }

        if (d->surface)
            initSurface();
    }

    Q_EMIT surfaceChanged();
}

QQuickItem *WSurfaceItem::contentItem() const
{
    Q_D(const WSurfaceItem);
    return d->contentItem;
}

QQuickItem *WSurfaceItem::eventItem() const
{
    Q_D(const WSurfaceItem);
    return d->eventItem;
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

void WSurfaceItem::componentComplete()
{
    Q_D(WSurfaceItem);

    if (d->surface) {
        initSurface();
    }

    QQuickItem::componentComplete();
}

void WSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(WSurfaceItem);

    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (d->resizeMode == SizeToSurface && newGeometry.size() != oldGeometry.size()) {
        if (resizeSurface(newGeometry.size().toSize()))
            d->contentItem->setSize(d->contentItem->size() + newGeometry.size() - oldGeometry.size());
    }
}

void WSurfaceItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    Q_D(WSurfaceItem);

    if (!d->componentComplete)
        return;

    if (change == ItemVisibleHasChanged) {
        if (d->surface)
            d->updateFrameDoneConnection();
    } else if (change == ItemChildRemovedChange) {
        // Don't use qobject_cast, because this item is in destroy,
        // Use static_cast to avoid convert failed.
        auto item = static_cast<WSurfaceItem*>(data.item);
        if (item && d->subsurfaces.removeOne(item)) {
            Q_EMIT subsurfaceRemoved(item);
        }
    }
}

void WSurfaceItem::focusInEvent(QFocusEvent *event)
{
    QQuickItem::focusInEvent(event);

    Q_D(WSurfaceItem);
    d->eventItem->forceActiveFocus(event->reason());
}

void WSurfaceItem::initSurface()
{
    Q_D(WSurfaceItem);

    d->initForSurface();
    d->contentItem->update();
}

bool WSurfaceItem::sendEvent(QInputEvent *event)
{
    Q_D(WSurfaceItem);
    if (!d->surface)
        return false;

    return WSeat::sendEvent(d->surface.get(), this, event);
}

void WSurfaceItem::onSurfaceCommit()
{
    Q_D(WSurfaceItem);

    const QRectF geometry = getContentGeometry();
    if (d->resizeMode == WSurfaceItem::SizeFromSurface) {
        if (!qFuzzyCompare(d->implicitWidth, geometry.width()))
            setImplicitWidth(geometry.width());
        if (!qFuzzyCompare(d->implicitHeight, geometry.height()))
            setImplicitHeight(geometry.height());
    }
    d->contentItem->setSize(d->surface->size());
    d->contentItem->setPosition(-geometry.topLeft());

    d->updateSubsurfaceItem();
}

bool WSurfaceItem::resizeSurface(const QSize &newSize)
{
    Q_UNUSED(newSize);
    return false;
}

QRectF WSurfaceItem::getContentGeometry() const
{
    Q_D(const WSurfaceItem);
    return QRectF(QPointF(0, 0), d->surface->size());
}

bool WSurfaceItem::inputRegionContains(const QPointF &position) const
{
    Q_D(const WSurfaceItem);
    return d->surface->inputRegionContains(position);
}

WSurfaceItemPrivate::WSurfaceItemPrivate()
{

}

WSurfaceItemPrivate::~WSurfaceItemPrivate()
{
    if (frameDoneConnection)
        QObject::disconnect(frameDoneConnection);
}

void WSurfaceItemPrivate::initForSurface()
{
    Q_Q(WSurfaceItem);

    contentItem->m_textureProvider->updateTexture();

    contentItem->m_updateTextureConnection = QObject::connect(surface, &WSurface::textureChanged,
                                                              contentItem->m_textureProvider,
                                                              &WSGTextureProvider::updateTexture);
    QObject::connect(surface, &WSurface::destroyed, q,
                     &WSurfaceItem::releaseResources, Qt::DirectConnection);
    QObject::connect(surface, &WSurface::primaryOutputChanged, q, [this] {
        updateFrameDoneConnection();
    });
    QObject::connect(surface, SIGNAL(hasSubsurfaceChanged()), q, SLOT(onHasSubsurfaceChanged()));
    QObject::connect(surface->handle(), &QWSurface::commit, q, &WSurfaceItem::onSurfaceCommit);

    onHasSubsurfaceChanged();
    updateFrameDoneConnection();
    q->onSurfaceCommit();
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

void WSurfaceItemPrivate::updateSubsurfaceItem()
{
    Q_Q(WSurfaceItem);
    auto surface = this->surface->handle()->handle();
    Q_ASSERT(surface);
    Q_ASSERT(contentItem);

    QQuickItem *prev = nullptr;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        WSurface *surface = WSurface::fromHandle(subsurface->surface);
        if (!surface)
            continue;
        WSurfaceItem *item = ensureSubsurfaceItem(surface);
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
        WSurface *surface = WSurface::fromHandle(subsurface->surface);
        if (!surface)
            continue;
        WSurfaceItem *item = ensureSubsurfaceItem(surface);
        Q_ASSERT(item->parentItem() == q);
        Q_ASSERT(prev->parentItem() == item->parentItem());
        item->stackAfter(prev);
        prev = item;
        item->setPosition(contentItem->position() + QPointF(subsurface->current.x, subsurface->current.y));
    }
}

WSurfaceItem *WSurfaceItemPrivate::ensureSubsurfaceItem(WSurface *subsurfaceSurface)
{
    for (int i = 0; i < subsurfaces.count(); ++i) {
        auto surfaceItem = subsurfaces.at(i);
        WSurface *surface = surfaceItem->d_func()->surface.get();

        if (surface && surface == subsurfaceSurface)
            return surfaceItem;
    }

    Q_Q(WSurfaceItem);
    Q_ASSERT(subsurfaceSurface);
    auto surfaceItem = new WSurfaceItem(q);
    QObject::connect(subsurfaceSurface->handle(), &QWSurface::beforeDestroy, surfaceItem, &WSurfaceItem::deleteLater);
    surfaceItem->setSurface(subsurfaceSurface);
    // remove list element in WSurfaceItem::itemChange
    subsurfaces.append(surfaceItem);
    Q_EMIT q->subsurfaceAdded(surfaceItem);

    return surfaceItem;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfaceitem.cpp"
#include "wsurfaceitem.moc"
