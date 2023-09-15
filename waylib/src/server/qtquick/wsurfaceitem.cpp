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
#include <qwtexture.h>
#include <qwbuffer.h>

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
    ~WSGTextureProvider();

public:
    QSGTexture *texture() const override;
    void updateTexture(); // in render thread
    void maybeUpdateTextureOnSurfacePrrimaryOutputChanged();
    void reset();

    QWTexture *ensureTexture();

private:
    ContentItem *item;
    QWBuffer *buffer = nullptr;
    std::unique_ptr<QWTexture> qwtexture;
    std::unique_ptr<WTexture> dwtexture;
};

struct SurfaceState {
    QRectF bufferSourceBox;
    QPoint bufferOffset;
    QRectF contentGeometry;
    QSizeF contentSize;
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
    void onPaddingsChanged();
    WSurfaceItem *ensureSubsurfaceItem(WSurface *subsurfaceSurface);

    void resizeSurfaceToItemSize(const QSize &itemSize, const QSize &sizeDiff);
    void updateEventItem(bool forceDestroy);

    inline QSizeF paddingsSize() const {
        return QSizeF(paddings.left() + paddings.right(),
                      paddings.top() + paddings.bottom());
    }

    Q_DECLARE_PUBLIC(WSurfaceItem)
    QPointer<WSurface> surface;
    std::unique_ptr<SurfaceState> surfaceState;
    ContentItem *contentItem = nullptr;
    QQuickItem *eventItem = nullptr;
    WSurfaceItem::ResizeMode resizeMode = WSurfaceItem::SizeFromSurface;
    WSurfaceItem::Flags surfaceFlags;
    QMarginsF paddings;
    QList<WSurfaceItem*> subsurfaces;

    QMetaObject::Connection frameDoneConnection;
    uint32_t beforeRequestResizeSurfaceStateSeq = 0;
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
        setAcceptTouchEvents(true);
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
        if (Q_UNLIKELY(!isValid()))
            return false;

        return d()->surface->inputRegionContains(point);
    }

private:
    bool event(QEvent *event) override {
        switch(event->type()) {
        using enum QEvent::Type;
        // Don't insert events before MouseButtonPress
        case MouseButtonPress: Q_FALLTHROUGH();
        case MouseButtonRelease: Q_FALLTHROUGH();
        case MouseMove: Q_FALLTHROUGH();
        case HoverMove:
            if (static_cast<QMouseEvent*>(event)->source() != Qt::MouseEventNotSynthesized)
                return true; // The non-native events don't send to WSeat
            Q_FALLTHROUGH();
        case HoverEnter: Q_FALLTHROUGH();
        case HoverLeave: Q_FALLTHROUGH();
        case KeyPress: Q_FALLTHROUGH();
        case KeyRelease: Q_FALLTHROUGH();
        case TouchBegin: Q_FALLTHROUGH();
        case TouchUpdate: Q_FALLTHROUGH();
        case TouchEnd: Q_FALLTHROUGH();
        case TouchCancel: {
            auto e = static_cast<QInputEvent*>(event);
            if (static_cast<ContentItem*>(parent())->surfaceItem()->sendEvent(e))
                return true;
            break;
        }
        case FocusOut: {
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
    if (!m_textureProvider || !m_textureProvider->texture() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        auto texture = m_textureProvider->texture();
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setTexture(texture);
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry = d()->surfaceState->bufferSourceBox;
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(d()->surfaceState->bufferOffset, size());
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

WSGTextureProvider::~WSGTextureProvider()
{
    if (buffer)
        buffer->unlock();
    buffer = nullptr;
}

QSGTexture *WSGTextureProvider::texture() const
{
    if (!buffer)
        return nullptr;

    return dwtexture->getSGTexture(item->window());
}

void WSGTextureProvider::updateTexture()
{
    if (qwtexture)
        qwtexture.reset();

    if (buffer)
        buffer->unlock();
    buffer = item->d()->surface->buffer();
    // lock buffer to ensure the WSurfaceItem can keep the last frame after WSurface destroyed.
    if (buffer)
        buffer->lock();

    dwtexture->setHandle(ensureTexture());
    Q_EMIT textureChanged();
    item->update();
}

void WSGTextureProvider::maybeUpdateTextureOnSurfacePrrimaryOutputChanged()
{
    // Maybe the last failure of WSGTextureProvider::ensureTexture() cause is
    // because the surface's primary output is nullptr.
    if (!dwtexture->handle()) {
        dwtexture->setHandle(ensureTexture());
        if (!dwtexture->handle()) {
            Q_EMIT textureChanged();
            item->update();
        }
    }
}

void WSGTextureProvider::reset()
{
    if (qwtexture)
        qwtexture.reset();
    if (buffer)
        buffer->unlock();
    buffer = nullptr;
    dwtexture->setHandle(nullptr);
    Q_EMIT textureChanged();
    item->update();
}

QWTexture *WSGTextureProvider::ensureTexture()
{
    auto textureHandle = item->d()->surface->handle()->getTexture();
    if (textureHandle)
        return textureHandle;

    if (qwtexture)
        return qwtexture.get();

    if (!buffer)
        return nullptr;

    auto output = item->d()->surface->primaryOutput();
    if (!output)
        return nullptr;

    auto renderer = output->renderer();
    if (!renderer)
        return nullptr;

    qwtexture.reset(QWTexture::fromBuffer(renderer, buffer));
    return qwtexture.get();
}

WSurfaceItem::WSurfaceItem(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemPrivate(), parent)
{
    setFlag(ItemIsFocusScope);

    Q_D(WSurfaceItem);
    auto contentItem = new ContentItem(this);
    d->contentItem = contentItem;
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
    d->beforeRequestResizeSurfaceStateSeq = 0;
    d->surface = surface;
    if (d->componentComplete) {
        if (oldSurface) {
            if (auto qwsurface = oldSurface->handle())
                qwsurface->disconnect(this);

            oldSurface->disconnect(this);
            oldSurface->disconnect(d->contentItem->m_textureProvider);
        }

        if (d->surface)
            initSurface();
    }

    if (!d->surface)
        releaseResources();

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

void WSurfaceItem::resize(ResizeMode mode)
{
    Q_ASSERT(mode != ManualResize);
    Q_D(WSurfaceItem);
    Q_ASSERT(d->surfaceState);

    if (!d->effectiveVisible)
        return;

    if (mode == SizeFromSurface) {
        const QSizeF content = d->surfaceState->contentGeometry.size() + d->paddingsSize();
        setSize(content);
    } else if (mode == SizeToSurface) {
        const QSizeF newSize = size() - d->paddingsSize();
        const QSizeF oldSize = d->surfaceState->contentGeometry.size();

        d->resizeSurfaceToItemSize(newSize.toSize(), (newSize - oldSize).toSize());
    } else {
        qWarning() << "Invalid resize mode" << mode;
    }
}

bool WSurfaceItem::effectiveVisible() const
{
    Q_D(const WSurfaceItem);
    return d->effectiveVisible;
}

WSurfaceItem::Flags WSurfaceItem::flags() const
{
    Q_D(const WSurfaceItem);
    return d->surfaceFlags;
}

void WSurfaceItem::setFlags(const Flags &newFlags)
{
    Q_D(WSurfaceItem);

    if (d->surfaceFlags == newFlags)
        return;
    d->surfaceFlags = newFlags;
    d->updateEventItem(false);

    Q_EMIT flagsChanged();
}

qreal WSurfaceItem::rightPadding() const
{
    Q_D(const WSurfaceItem);
    return d->paddings.right();
}

void WSurfaceItem::setRightPadding(qreal newRightPadding)
{
    Q_D(WSurfaceItem);
    if (qFuzzyCompare(d->paddings.right(), newRightPadding))
        return;
    d->paddings.setRight(newRightPadding);
    d->onPaddingsChanged();
    Q_EMIT rightPaddingChanged();
}

qreal WSurfaceItem::leftPadding() const
{
    Q_D(const WSurfaceItem);
    return d->paddings.left();
}

void WSurfaceItem::setLeftPadding(qreal newLeftPadding)
{
    Q_D(WSurfaceItem);
    if (qFuzzyCompare(d->paddings.left(), newLeftPadding))
        return;
    d->paddings.setLeft(newLeftPadding);
    d->onPaddingsChanged();
    Q_EMIT leftPaddingChanged();
}

qreal WSurfaceItem::bottomPadding() const
{
    Q_D(const WSurfaceItem);
    return d->paddings.bottom();
}

void WSurfaceItem::setBottomPadding(qreal newBottomPadding)
{
    Q_D(WSurfaceItem);
    if (qFuzzyCompare(d->paddings.bottom(), newBottomPadding))
        return;
    d->paddings.setBottom(newBottomPadding);
    d->onPaddingsChanged();
    Q_EMIT bottomPaddingChanged();
}

qreal WSurfaceItem::topPadding() const
{
    Q_D(const WSurfaceItem);
    return d->paddings.top();
}

void WSurfaceItem::setTopPadding(qreal newTopPadding)
{
    Q_D(WSurfaceItem);
    if (qFuzzyCompare(d->paddings.top(), newTopPadding))
        return;
    d->paddings.setTop(newTopPadding);
    d->onPaddingsChanged();
    Q_EMIT topPaddingChanged();
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

    if (newGeometry.size() == oldGeometry.size())
        return;

    if (d->resizeMode == SizeToSurface) {
        if (d->effectiveVisible) {
            const QSizeF newSize = newGeometry.size();
            const QSizeF oldSize = oldGeometry.size();

            d->resizeSurfaceToItemSize((newSize - d->paddingsSize()).toSize(),
                                       (newSize - oldSize).toSize());
        }
    } else if (!d->surface && d->resizeMode != ManualResize) {
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
        if (d->surface) {
            d->updateFrameDoneConnection();

            if (d->effectiveVisible) {
                if (d->resizeMode != ManualResize)
                    resize(d->resizeMode);
                d->contentItem->setSize(d->surfaceState->contentSize);
            }
        }

        Q_EMIT effectiveVisibleChanged();
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
    if (d->eventItem)
        d->eventItem->forceActiveFocus(event->reason());
}

void WSurfaceItem::releaseResources()
{
    Q_D(WSurfaceItem);

    d->beforeRequestResizeSurfaceStateSeq = 0;

    if (d->contentItem->m_updateTextureConnection)
        QObject::disconnect(d->contentItem->m_updateTextureConnection);

    if (d->frameDoneConnection)
        QObject::disconnect(d->frameDoneConnection);

    if (d->surface) {
        d->surface->disconnect(this);
        if (auto qwsurface = d->surface->handle())
            qwsurface->disconnect(this);
    }

    if (!d->surfaceFlags.testFlag(DontCacheLastBuffer)) {
        for (auto item : d->subsurfaces) {
            item->releaseResources();
            // Don't auto destroy subsurfaes's items, ensure save the
            // subsurface's contents at the last frame buffer.
            QObject::disconnect(item->surface(), &QWSurface::destroyed,
                                item, &WSurfaceItem::deleteLater);
        }
    } else {
        if (d->contentItem->m_textureProvider)
            d->contentItem->m_textureProvider->reset();

        for (auto item : d->subsurfaces)
            item->deleteLater();
    }

    d->updateEventItem(true);
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

    return WSeat::sendEvent(d->surface.get(), this, d->eventItem, event);
}

void WSurfaceItem::onSurfaceCommit()
{
    Q_D(WSurfaceItem);

    updateSurfaceState();

    // Maybe the beforeRequestResizeSurfaceStateSeq is set by resizeSurfaceToItemSize,
    // the resizeSurfaceToItemSize wants to resize the wl_surface to current size of WSurfaceitem,
    // If change the WSurfaceItem's size at here, you will see the WSurfaceItem flash.
    if (d->beforeRequestResizeSurfaceStateSeq < d->surface->handle()->handle()->current.seq) {
        if (d->beforeRequestResizeSurfaceStateSeq != 0) {
            Q_ASSERT(d->beforeRequestResizeSurfaceStateSeq == d->surface->handle()->handle()->current.seq - 1);
            d->beforeRequestResizeSurfaceStateSeq = 0;
        }

        if (d->resizeMode == WSurfaceItem::SizeFromSurface)
            resize(d->resizeMode);

        if (d->effectiveVisible)
            d->contentItem->setSize(d->surfaceState->contentSize);
        d->contentItem->setPosition(-d->surfaceState->contentGeometry.topLeft()
                                    + QPointF(d->paddings.left(), d->paddings.top()));
    }

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

QSizeF WSurfaceItem::getContentSize() const
{
    Q_D(const WSurfaceItem);
    return d->surface->size();
}

bool WSurfaceItem::inputRegionContains(const QPointF &position) const
{
    Q_D(const WSurfaceItem);
    Q_ASSERT(d->surface);
    return d->surface->inputRegionContains(position);
}

void WSurfaceItem::updateSurfaceState()
{
    Q_D(WSurfaceItem);

    if (Q_LIKELY(d->surface)) {
        d->surfaceState->bufferSourceBox = d->surface->handle()->getBufferSourceBox();
        d->surfaceState->bufferOffset = d->surface->bufferOffset();
    }
    d->surfaceState->contentGeometry = getContentGeometry();
    d->surfaceState->contentSize = getContentSize();
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

    // clean subsurfaces, if cacheLastBuffer is enabled will cache
    // the previous WSurface's subsurfaces.
    for (auto item : subsurfaces)
        item->deleteLater();
    subsurfaces.clear();

    if (!surfaceState)
        surfaceState.reset(new SurfaceState());

    contentItem->m_textureProvider->updateTexture();
    contentItem->m_updateTextureConnection = QObject::connect(surface, &WSurface::bufferChanged,
                                                              contentItem->m_textureProvider,
                                                              &WSGTextureProvider::updateTexture);
    QObject::connect(surface->handle(), &QWSurface::beforeDestroy, q,
                     &WSurfaceItem::releaseResources, Qt::DirectConnection);
    QObject::connect(surface, &WSurface::primaryOutputChanged, q, [this] {
        updateFrameDoneConnection();

        if (contentItem->m_textureProvider)
            contentItem->m_textureProvider->maybeUpdateTextureOnSurfacePrrimaryOutputChanged();
    });
    QObject::connect(surface, SIGNAL(hasSubsurfaceChanged()), q, SLOT(onHasSubsurfaceChanged()));
    QObject::connect(surface->handle(), &QWSurface::commit, q, &WSurfaceItem::onSurfaceCommit);

    onHasSubsurfaceChanged();
    updateFrameDoneConnection();
    updateEventItem(false);
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

void WSurfaceItemPrivate::onPaddingsChanged()
{
    W_Q(WSurfaceItem);

    if (!surface || !surfaceState)
        return;

    if (resizeMode != WSurfaceItem::ManualResize)
        q->resize(resizeMode);
    contentItem->setPosition(-surfaceState->contentGeometry.topLeft()
                             + QPointF(paddings.left(), paddings.top()));

    // subsurfae need contentItem's position
    updateSubsurfaceItem();
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
    // Delay destroy WSurfaceItem, because if the cause of destroy is because the parent
    // surface destroy, and the parent WSurfaceItem::cacheLastBuffer maybe enabled,
    // will disable this connection at parent WSurfaceItem::releaseResources to save the
    // parent WSurface last frame, the last frame contents should include its subsurfaces's
    // contents.
    QObject::connect(subsurfaceSurface, &QWSurface::destroyed,
                     surfaceItem, &WSurfaceItem::deleteLater, Qt::QueuedConnection);
    surfaceItem->setSurface(subsurfaceSurface);
    // remove list element in WSurfaceItem::itemChange
    subsurfaces.append(surfaceItem);
    Q_EMIT q->subsurfaceAdded(surfaceItem);

    return surfaceItem;
}

void WSurfaceItemPrivate::resizeSurfaceToItemSize(const QSize &itemSize, const QSize &sizeDiff)
{
    Q_Q(WSurfaceItem);

    Q_ASSERT_X(itemSize == (q->size() - paddingsSize()).toSize(), "WSurfaceItem",
               "The function only using for reisze wl_surface's size to the WSurfaceItem's current size");

    if (!surface) {
        contentItem->setSize(contentItem->size() + sizeDiff);
        return;
    }

    if (q->resizeSurface(itemSize)) {
        contentItem->setSize(contentItem->size() + sizeDiff);
        beforeRequestResizeSurfaceStateSeq = surface->handle()->handle()->pending.seq;
    }
}

void WSurfaceItemPrivate::updateEventItem(bool forceDestroy)
{
    const bool needsEventItem = !forceDestroy && !surfaceFlags.testFlag(WSurfaceItem::RejectEvent);
    if (bool(eventItem) == needsEventItem)
        return;

    if (eventItem) {
        eventItem->setVisible(false);
        eventItem->setParentItem(nullptr);
        eventItem->setParent(nullptr);
        delete eventItem;
        eventItem = nullptr;
    } else {
        eventItem = new EventItem(contentItem);
        QQuickItemPrivate::get(eventItem)->anchors()->setFill(contentItem);
    }

    Q_EMIT q_func()->eventItemChanged();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfaceitem.cpp"
#include "wsurfaceitem.moc"
