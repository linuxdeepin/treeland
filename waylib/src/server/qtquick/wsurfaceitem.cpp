// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wsurface.h"
#include "wtexture.h"
#include "wseat.h"
#include "wcursor.h"
#include "woutput.h"
#include "woutputviewport.h"
#include "wbuffertextureprovider.h"

#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwtexture.h>
#include <qwbuffer.h>

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRenderNode>
#include <private/qquickitem_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGTextureProvider : public WBufferTextureProvider
{
    friend class WSurfaceItemContent;
public:
    WSGTextureProvider(WSurfaceItemContent *item);
    ~WSGTextureProvider();

public:
    QSGTexture *texture() const override;
    QWTexture *qwTexture() const override;
    QWBuffer *qwBuffer() const override;
    void updateTexture(); // in render thread
    void tryUpdateTexture();
    void maybeUpdateTextureOnSurfacePrrimaryOutputChanged();
    void reset();

    QWTexture *ensureTexture();

private:
    void doUpdateTexture();
    WSurfaceItemContent *item;
    QWBuffer *buffer = nullptr;
    std::unique_ptr<QWTexture> qwtexture;
    std::unique_ptr<WTexture> dwtexture;
    bool textureDirty = false;
};

class EventItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit EventItem(WSurfaceItem *parent)
        : QQuickItem(parent) {
        setAcceptHoverEvents(true);
        setAcceptTouchEvents(true);
        setAcceptedMouseButtons(Qt::AllButtons);
        setFlag(QQuickItem::ItemClipsChildrenToShape, true);
    }

    inline bool isValid() const {
        if (!parent())
            return false;

        auto surface = static_cast<WSurfaceItem*>(parent())->surface();
        return surface && !surface->isInvalidated();
    }

    inline WSurfaceItemPrivate *d() const {
        return WSurfaceItemPrivate::get(static_cast<WSurfaceItem*>(parent()));
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
        case Wheel: Q_FALLTHROUGH();
        case TouchBegin: Q_FALLTHROUGH();
        case TouchUpdate: Q_FALLTHROUGH();
        case TouchEnd: Q_FALLTHROUGH();
        case TouchCancel: {
            auto e = static_cast<QInputEvent*>(event);
            Q_ASSERT(e);
            // We may receive HoverLeave when WSurfaceItem was destroying
            if (parent() && static_cast<WSurfaceItem*>(parent())->sendEvent(e))
                return true;
            break;
        }
        case NativeGesture: {
            auto e = static_cast<QNativeGestureEvent*>(event);
            Q_ASSERT(e);
            if (parent() && static_cast<WSurfaceItem*>(parent())->sendEvent(e))
                return true;
            break;
        }
        // No need to process focusOut, see [PR 282](https://github.com/vioken/waylib/pull/282)
        default:
            break;
        }

        return QQuickItem::event(event);
    }
};

class WSurfaceItemContentPrivate: public QQuickItemPrivate
{
public:
    WSurfaceItemContentPrivate(WSurfaceItemContent *qq){}

    ~WSurfaceItemContentPrivate() {
        if (updateTextureConnection) {
            Q_ASSERT(surface);
            surface->safeDisconnect(updateTextureConnection);
        }

        if (frameDoneConnection)
            QObject::disconnect(frameDoneConnection);

        if (textureProvider) {
            delete textureProvider;
            textureProvider = nullptr;
        }
    }

    inline WSGTextureProvider *tp() const {
        if (!textureProvider) {
            Q_ASSERT(surface);
            textureProvider = new WSGTextureProvider(const_cast<WSurfaceItemContent*>(q_func()));
            updateTextureConnection = surface->safeConnect(&WSurface::bufferChanged,
                                                           textureProvider,
                                                           &WSGTextureProvider::updateTexture);
        }
        return textureProvider;
    }

    void invalidate() {
        if (surface) {
            W_Q(WSurfaceItemContent);
            surface->safeDisconnect(q);
            if (textureProvider) {
                surface->safeDisconnect(textureProvider);
            }
            surface = nullptr;
        }

        if (frameDoneConnection)
            QObject::disconnect(frameDoneConnection);

        Q_ASSERT(!updateTextureConnection);

        if (dontCacheLastBuffer && textureProvider)
            textureProvider->reset();
    }

    void init() {
        W_Q(WSurfaceItemContent);

        surface->safeConnect(&QWSurface::commit, q, [this] {
            updateSurfaceState();
        });
        surface->safeConnect(&WSurface::primaryOutputChanged, q, [this] {
            if (textureProvider)
                textureProvider->maybeUpdateTextureOnSurfacePrrimaryOutputChanged();
        });

        if (textureProvider) {
            Q_ASSERT(!updateTextureConnection);
            updateTextureConnection = surface->safeConnect(&WSurface::bufferChanged,
                                                           textureProvider,
                                                           &WSGTextureProvider::updateTexture);
        }

        updateFrameDoneConnection();
        updateSurfaceState();
        tp()->updateTexture();
        q->rendered = true;
    }

    void updateFrameDoneConnection() {
        W_Q(WSurfaceItemContent);

        if (frameDoneConnection)
            QObject::disconnect(frameDoneConnection);
        if (!q->window()) // maybe null due to item not fully initialized
            return;

        // wayland protocol job should not run in rendering thread, so set context qobject to contentItem
        frameDoneConnection = QObject::connect(q->window(), &QQuickWindow::afterRendering, q, [this, q](){
            if (q->rendered && live) {
                surface->notifyFrameDone();
                q->rendered = false;
            }
        }); // if signal is emitted from seperated rendering thread, default QueuedConnection is used
    }

    void updateSurfaceState() {
        if (!surface)
            return;

        bufferSourceBox = surface->handle()->getBufferSourceBox();
        bufferOffset = surface->bufferOffset();
    }

    W_DECLARE_PUBLIC(WSurfaceItemContent)
    QPointer<WSurface> surface;
    QRectF bufferSourceBox;
    QPoint bufferOffset;

    QMetaObject::Connection frameDoneConnection;
    mutable WSGTextureProvider *textureProvider = nullptr;
    mutable QMetaObject::Connection updateTextureConnection;
    bool dontCacheLastBuffer = false;
    bool live = true;
};


WSurfaceItemContent::WSurfaceItemContent(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemContentPrivate(this), parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

WSurface *WSurfaceItemContent::surface() const
{
    W_DC(WSurfaceItemContent);
    return d->surface;
}

void WSurfaceItemContent::setSurface(WSurface *surface)
{
    W_D(WSurfaceItemContent);

    // when surface is null and original surface has destroyed also need reset
    if (surface && d->surface == surface)
        return;

    auto oldSurface = d->surface;
    d->surface = surface;
    if (isComponentComplete()) {
        if (oldSurface) {
            oldSurface->safeDisconnect(this);
            if (d->textureProvider)
                oldSurface->safeDisconnect(d->textureProvider);
        }

        if (d->surface)
            d->init();
    }

    if (!d->surface)
        d->invalidate();

    Q_EMIT surfaceChanged();
}

bool WSurfaceItemContent::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *WSurfaceItemContent::textureProvider() const
{
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    W_DC(WSurfaceItemContent);
    return d->tp();
}

WBufferTextureProvider *WSurfaceItemContent::wTextureProvider() const
{
    W_DC(WSurfaceItemContent);
    return d->tp();
}

bool WSurfaceItemContent::cacheLastBuffer() const
{
    W_DC(WSurfaceItemContent);
    return !d->dontCacheLastBuffer;
}

void WSurfaceItemContent::setCacheLastBuffer(bool newCacheLastBuffer)
{
    W_D(WSurfaceItemContent);
    if (d->dontCacheLastBuffer == !newCacheLastBuffer)
        return;
    d->dontCacheLastBuffer = !newCacheLastBuffer;
    Q_EMIT cacheLastBufferChanged();
}

bool WSurfaceItemContent::live() const
{
    W_DC(WSurfaceItemContent);
    return d->live;
}

void WSurfaceItemContent::setLive(bool live)
{
    W_D(WSurfaceItemContent);
    if (d->live == live)
        return;
    d->live = live;
    if (live && d->textureProvider)
        d->textureProvider->tryUpdateTexture();
    Q_EMIT liveChanged();
}

void WSurfaceItemContent::componentComplete()
{
    QQuickItem::componentComplete();

    W_D(WSurfaceItemContent);
    if (d->surface)
        d->init();
}

class WSGRenderFootprintNode: public QSGRenderNode
{
public:
    WSGRenderFootprintNode(WSurfaceItemContent *owner)
        : QSGRenderNode()
        , m_owner(owner)
    {
        setFlag(QSGNode::OwnedByParent); // parent is fixed, auto release
    }

    ~WSGRenderFootprintNode() {}

    void render(const RenderState*) override
    {
        if (Q_LIKELY(m_owner))
            m_owner->rendered = true;
    }

    QPointer<WSurfaceItemContent> m_owner;
};

QSGNode *WSurfaceItemContent::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto privt = QQuickItemPrivate::get(this);
    W_D(WSurfaceItemContent);
    if (!d->textureProvider || !d->textureProvider->texture() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGImageNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        auto texture = d->textureProvider->texture();
        node = window()->createImageNode();
        node->setOwnsTexture(false);
        node->setTexture(texture);
        QSGNode *fpnode = new WSGRenderFootprintNode(this);
        node->appendChildNode(fpnode);
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry = d->bufferSourceBox;
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(d->bufferOffset, size());
    node->setRect(targetGeometry);
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);

    return node;
}

void WSurfaceItemContent::releaseResources()
{
    W_D(WSurfaceItemContent);

    if (d->textureProvider) {
        class WSurfaceItemContentCleanupJob : public QRunnable
        {
        public:
            WSurfaceItemContentCleanupJob(QObject *object) : m_object(object) { }
            void run() override {
                delete m_object;
            }
            QObject *m_object;
        };

        // Delay clean the textures on the next render after.
        window()->scheduleRenderJob(new WSurfaceItemContentCleanupJob(d->textureProvider),
                                    QQuickWindow::AfterRenderingStage);
        d->textureProvider->item = nullptr;
        d->textureProvider = nullptr;
    }

    d->invalidate();

    // Force to update the contents, avoid to render the invalid textures
    QQuickItemPrivate::get(this)->dirty(QQuickItemPrivate::Content);
}

void WSurfaceItemContent::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    W_D(WSurfaceItemContent);
    if (change == QQuickItem::ItemSceneChange) {
        d->updateFrameDoneConnection();
    }
}

void WSurfaceItemContent::invalidateSceneGraph()
{
    W_D(WSurfaceItemContent);
    if (d->textureProvider)
        delete d->textureProvider;
    d->textureProvider = nullptr;
}

WSGTextureProvider::WSGTextureProvider(WSurfaceItemContent *item)
    : item(item)
{
    dwtexture.reset(new WTexture(nullptr));
    dwtexture->setOwnsTexture(false);
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

QWTexture *WSGTextureProvider::qwTexture() const
{
    if (!buffer)
        return nullptr;
    return qwtexture.get();
}

QWBuffer *WSGTextureProvider::qwBuffer() const
{
    return buffer;
}

void WSGTextureProvider::updateTexture()
{
    if (!item->live()) {
        textureDirty = true;
        return;
    }
    doUpdateTexture();
}

void WSGTextureProvider::doUpdateTexture()
{
    if (qwtexture)
        qwtexture.reset();

    if (buffer)
        buffer->unlock();
    buffer = item->d_func()->surface->buffer();
    // lock buffer to ensure the WSurfaceItem can keep the last frame after WSurface destroyed.
    if (buffer)
        buffer->lock();

    dwtexture->setHandle(ensureTexture());
    Q_EMIT textureChanged();
    item->update();
}

void WSGTextureProvider::tryUpdateTexture()
{
    if (textureDirty) {
        textureDirty = false;
        doUpdateTexture();
    }
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
    auto textureHandle = item->d_func()->surface->handle()->getTexture();
    if (textureHandle)
        return textureHandle;

    if (qwtexture)
        return qwtexture.get();

    if (!buffer)
        return nullptr;

    auto output = item->d_func()->surface->primaryOutput();
    if (!output)
        return nullptr;

    auto renderer = output->renderer();
    if (!renderer)
        return nullptr;

    qwtexture.reset(QWTexture::fromBuffer(renderer, buffer));
    return qwtexture.get();
}

WSurfaceItem::WSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WSurfaceItemPrivate(), parent)
{
}

WSurfaceItem::WSurfaceItem(WSurfaceItemPrivate &dd, QQuickItem *parent)
    : QQuickItem(dd, parent)
{
    setFlag(ItemIsFocusScope);
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

WSurface *WSurfaceItem::surface() const
{
    Q_D(const WSurfaceItem);
    return d->surface.get();
}

void WSurfaceItem::setSurface(WSurface *surface)
{
    Q_D(WSurfaceItem);

    // when surface is null and original surface has destroyed also need reset
    if (surface && d->surface == surface)
        return;

    auto oldSurface = d->surface;
    d->beforeRequestResizeSurfaceStateSeq = 0;
    d->surface = surface;
    if (d->componentComplete) {
        if (oldSurface) {
            oldSurface->safeDisconnect(this);
        }

        if (d->surface)
            initSurface();
    }

    // TODO: reset subsurfaces?

    if (!d->surface)
        releaseResources();

    if (auto content = d->getItemContent())
        content->setSurface(surface);

    Q_EMIT surfaceChanged();
}

QQuickItem *WSurfaceItem::contentItem() const
{
    Q_D(const WSurfaceItem);
    return d->contentContainer;
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
    Q_D(WSurfaceItem);

    if (mode == ManualResize) {
        qmlWarning(this) << "Can't resize WSurfaceItem for ManualResize mode.";
        return;
    }

    if (!d->surfaceState)
        return;

    if (!d->effectiveVisible)
        return;

    d->doResize(mode);
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

    if (auto content = d->getItemContent())
        content->setCacheLastBuffer(!newFlags.testFlag(DontCacheLastBuffer));

    for (auto sub : std::as_const(d->subsurfaces))
        sub->setFlags(newFlags);

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
    d->implicitWidthChanged();
    Q_EMIT rightPaddingChanged();
}

qreal WSurfaceItem::surfaceSizeRatio() const
{
    Q_D(const WSurfaceItem);
    return d->surfaceSizeRatio;
}

void WSurfaceItem::setSurfaceSizeRatio(qreal ssr)
{
    Q_D(WSurfaceItem);

    if (qFuzzyCompare(ssr, d->surfaceSizeRatio))
        return;
    d->surfaceSizeRatio = ssr;
    Q_EMIT surfaceSizeRatioChanged();

    surfaceSizeRatioChange();
}

qreal WSurfaceItem::bufferScale() const
{
    Q_D(const WSurfaceItem);

    return d->surfaceState ? d->surfaceState->bufferScale : 1.0;
}

QQmlComponent *WSurfaceItem::delegate() const
{
    Q_D(const WSurfaceItem);

    return d->delegate;
}

void WSurfaceItem::setDelegate(QQmlComponent *newDelegate)
{
    Q_D(WSurfaceItem);

    if (d->delegate == newDelegate)
        return;
    d->delegate = newDelegate;
    d->initForDelegate();

    for (auto sub : std::as_const(d->subsurfaces))
        sub->setDelegate(newDelegate);

    Q_EMIT delegateChanged();
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
    d->implicitWidthChanged();
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
    d->implicitHeightChanged();
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
    d->implicitHeightChanged();
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

            d->resizeSurfaceToItemSize(((newSize - d->paddingsSize()) * d->surfaceSizeRatio).toSize(),
                                       ((newSize - oldSize) * d->surfaceSizeRatio).toSize());
        }
    } else if (!d->surface && d->resizeMode != ManualResize) {
        d->contentContainer->setSize(d->contentContainer->size() +
                                     (newGeometry.size() - oldGeometry.size()) * d->surfaceSizeRatio);
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
            if (d->effectiveVisible) {
                if (d->resizeMode != ManualResize)
                    d->doResize(d->resizeMode);
                d->contentContainer->setSize(d->surfaceState->contentSize);
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

    if (d->surface) {
        d->surface->safeDisconnect(this);
    }

    if (!d->surfaceFlags.testFlag(DontCacheLastBuffer)) {
        for (auto item : std::as_const(d->subsurfaces)) {
            item->releaseResources();
            // subsurface's contents at the last frame buffer.
            // AutoDestroy: disconnects (subsurfaceItem.surface, destroyed, this, lambda{deleteLater})
            bool disconnAutoDestroy = QObject::disconnect(item->surface(), &WSurface::destroyed, this, nullptr);
            Q_ASSERT( disconnAutoDestroy || item->property("_autoDestroyReleased").toBool() );
            item->setProperty("_autoDestroyReleased", true);
        }
    } else {
        for (auto item : std::as_const(d->subsurfaces))
            item->deleteLater();
    }

    if (auto content = d->getItemContent())
        content->d_func()->invalidate();

    d->updateEventItem(true);
}

void WSurfaceItem::initSurface()
{
    Q_D(WSurfaceItem);

    d->initForDelegate();
    d->initForSurface();
}

bool WSurfaceItem::sendEvent(QInputEvent *event)
{
    Q_D(WSurfaceItem);
    if (!d->surface)
        return false;

    return WSeat::sendEvent(d->surface.get(), this, d->eventItem, event);
}

bool WSurfaceItem::doResizeSurface(const QSize &newSize)
{
    Q_D(WSurfaceItem);
    Q_ASSERT(d->shellSurface);
    d->shellSurface->resize(newSize);
    return true;
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

        if (d->effectiveVisible) {
            if (d->resizeMode == WSurfaceItem::SizeFromSurface)
                d->doResize(d->resizeMode);

            d->contentContainer->setSize(d->surfaceState->contentSize);
        }
        d->updateContentPosition();
    }

    d->updateSubsurfaceItem();
}

bool WSurfaceItem::resizeSurface(const QSizeF &newSize)
{
    Q_D(const WSurfaceItem);
    if (!d->shellSurface || !d->contentContainer)
        return false;
    const QRectF tmp(0, 0, newSize.width(), newSize.height());
    // See surfaceSizeRatio, the content item maybe has been scaled.
    const QSize mappedSize = d->contentContainer->mapRectFromItem(this, tmp).size().toSize();
    if (!d->shellSurface->checkNewSize(mappedSize))
        return false;
    return doResizeSurface(mappedSize);
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

void WSurfaceItem::surfaceSizeRatioChange()
{
    Q_D(WSurfaceItem);

    if (d->resizeMode != ManualResize)
        resize(d->resizeMode);

    d->contentContainer->setTransformOrigin(QQuickItem::TopLeft);
    d->contentContainer->setScale(1.0 / d->surfaceSizeRatio);

    if (d->surfaceState) {
        d->updateContentPosition();
    }

    if (d->eventItem) {
        d->updateEventItemGeometry();
    }

    if (d->surface) {
        d->updateSubsurfaceItem();
    }
}

void WSurfaceItem::updateSurfaceState()
{
    Q_D(WSurfaceItem);

    bool bufferScaleChanged = false;
    if (Q_LIKELY(d->surface)) {
        bufferScaleChanged = !qFuzzyCompare(d->surfaceState->bufferScale, d->surface->bufferScale());
        d->surfaceState->bufferScale = d->surface->bufferScale();
    }

    auto oldSize = d->surfaceState->contentGeometry.size();
    d->surfaceState->contentGeometry = getContentGeometry();
    d->surfaceState->contentSize = getContentSize();

    if (!qFuzzyCompare(oldSize.width(), d->surfaceState->contentGeometry.width()))
        implicitWidthChanged();
    if (!qFuzzyCompare(oldSize.height(), d->surfaceState->contentGeometry.height()))
        implicitHeightChanged();

    if (bufferScaleChanged)
        Q_EMIT this->bufferScaleChanged();
}

WSurfaceItemPrivate::WSurfaceItemPrivate()
{
    smooth = true;
}

WSurfaceItemPrivate::~WSurfaceItemPrivate()
{

}

void WSurfaceItemPrivate::initForSurface()
{
    Q_Q(WSurfaceItem);

    // clean subsurfaces, if cacheLastBuffer is enabled will cache
    // the previous WSurface's subsurfaces.
    for (auto item : std::as_const(subsurfaces))
        item->deleteLater();
    subsurfaces.clear();

    if (!surfaceState)
        surfaceState.reset(new SurfaceState());

    QObject::connect(surface, &WWrapObject::aboutToBeInvalidated, q,
                     &WSurfaceItem::releaseResources, Qt::DirectConnection);
    surface->safeConnect(&WSurface::hasSubsurfaceChanged, q, [this]{ onHasSubsurfaceChanged(); });
    surface->safeConnect(&QWSurface::commit, q, &WSurfaceItem::onSurfaceCommit);

    onHasSubsurfaceChanged();
    updateEventItem(false);
    q->onSurfaceCommit();
}

void WSurfaceItemPrivate::initForDelegate()
{
    Q_Q(WSurfaceItem);

    QQuickItem *newContentContainer = nullptr;

    if (!delegate) {
        if (getItemContent())
            return;

        auto contentItem = new WSurfaceItemContent(q);
        if (surface)
            contentItem->setSurface(surface);
        contentItem->setCacheLastBuffer(!surfaceFlags.testFlag(WSurfaceItem::DontCacheLastBuffer));
        contentItem->setSmooth(q->smooth());
        QObject::connect(q, &WSurfaceItem::smoothChanged, contentItem, &WSurfaceItemContent::setSmooth);
        newContentContainer = contentItem;
    } else {
        if (!contentContainer || getItemContent()) {
            newContentContainer = new QQuickItem(q);
        } else {
            newContentContainer = contentContainer;
        }

        auto obj = delegate->createWithInitialProperties({{"surface", QVariant::fromValue(q)}}, qmlContext(q));
        if (!obj) {
            qWarning() << "Failed on create surface item from delegate, error mssage:"
                       << delegate->errorString();
            return;
        }

        auto contentItem = qobject_cast<QQuickItem*>(obj);
        if (!contentItem)
            qFatal() << "SurfaceItem's delegate must is Item";

        QQmlEngine::setObjectOwnership(contentItem, QQmlEngine::CppOwnership);
        contentItem->setParentItem(newContentContainer);
    }

    if (newContentContainer == contentContainer)
        return;

    newContentContainer->setZ(qreal(WSurfaceItem::ZOrder::ContentItem));

    if (contentContainer) {
        newContentContainer->setPosition(contentContainer->position());
        newContentContainer->setSize(contentContainer->size());
        newContentContainer->setTransformOrigin(contentContainer->transformOrigin());
        newContentContainer->setScale(contentContainer->scale());

        contentContainer->disconnect(q);
        contentContainer->deleteLater();
    }
    contentContainer = newContentContainer;
    updateEventItem(false);
    if (eventItem)
        updateEventItemGeometry();

    Q_EMIT q->contentItemChanged();
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
    Q_ASSERT(contentContainer);

    QQuickItem *prev = nullptr;
    wlr_subsurface *subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        WSurface *surface = WSurface::fromHandle(subsurface->surface);
        if (!surface)
            continue;
        WSurfaceItem *item = ensureSubsurfaceItem(surface);
        item->setZ(qreal(WSurfaceItem::ZOrder::BelowSubsurface));
        item->setSurfaceSizeRatio(surfaceSizeRatio);
        Q_ASSERT(item->parentItem() == q);
        if (prev) {
            Q_ASSERT(prev->parentItem() == item->parentItem());
            item->stackAfter(prev);
        }
        prev = item;
        const QPointF pos = contentContainer->position() + QPointF(subsurface->current.x, subsurface->current.y) / surfaceSizeRatio;
        item->setPosition(pos);
    }

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        WSurface *surface = WSurface::fromHandle(subsurface->surface);
        if (!surface)
            continue;
        WSurfaceItem *item = ensureSubsurfaceItem(surface);
        item->setZ(qreal(WSurfaceItem::ZOrder::AboveSubsurface));
        item->setSurfaceSizeRatio(surfaceSizeRatio);
        Q_ASSERT(item->parentItem() == q);
        if (prev) {
            Q_ASSERT(prev->parentItem() == item->parentItem());
            item->stackAfter(prev);
        }
        prev = item;
        const QPointF pos = contentContainer->position() + QPointF(subsurface->current.x, subsurface->current.y) / surfaceSizeRatio;
        item->setPosition(pos);
    }
}

void WSurfaceItemPrivate::onPaddingsChanged()
{
    W_Q(WSurfaceItem);

    if (!surface || !surfaceState)
        return;

    if (resizeMode != WSurfaceItem::ManualResize && effectiveVisible)
        doResize(resizeMode);
    updateContentPosition();

    // subsurfae need contentContainer's position
    updateSubsurfaceItem();
}

void WSurfaceItemPrivate::updateContentPosition()
{
    Q_ASSERT(surfaceState);
    contentContainer->setPosition(-surfaceState->contentGeometry.topLeft() / surfaceSizeRatio
                                  + QPointF(paddings.left(), paddings.top()));
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
    // AutoDestroy: Connect to this(parent)'s lambda since the autodestroy is managed by parent,
    // avoids disconnected with all slots on subsurfaceItem in subsurface's releaseResources
    subsurfaceSurface->safeConnect(&WSurface::destroyed,
                             q, [this,surfaceItem]{ surfaceItem->deleteLater(); }, Qt::QueuedConnection);
    surfaceItem->setDelegate(delegate);
    surfaceItem->setFlags(surfaceFlags);
    surfaceItem->setSurface(subsurfaceSurface);
    surfaceItem->setSmooth(q->smooth());
    QObject::connect(q, &WSurfaceItem::smoothChanged, surfaceItem, &WSurfaceItem::setSmooth);
    // remove list element in WSurfaceItem::itemChange
    subsurfaces.append(surfaceItem);
    Q_EMIT q->subsurfaceAdded(surfaceItem);

    return surfaceItem;
}

void WSurfaceItemPrivate::resizeSurfaceToItemSize(const QSize &itemSize, const QSize &sizeDiff)
{
    Q_Q(WSurfaceItem);

    Q_ASSERT_X(itemSize == ((q->size() - paddingsSize()) * surfaceSizeRatio).toSize(), "WSurfaceItem",
               "The function only using for reisze wl_surface's size to the WSurfaceItem's current size");

    if (!surface) {
        contentContainer->setSize(contentContainer->size() + sizeDiff);
        return;
    }

    if (q->resizeSurface(itemSize)) {
        contentContainer->setSize(contentContainer->size() + sizeDiff);
        beforeRequestResizeSurfaceStateSeq = surface->handle()->handle()->pending.seq;
    }
}

void WSurfaceItemPrivate::updateEventItem(bool forceDestroy)
{
    auto contentItem = getItemContent();
    const bool needsEventItem = contentItem && !forceDestroy
                                && !surfaceFlags.testFlag(WSurfaceItem::RejectEvent);
    if (bool(eventItem) == needsEventItem)
        return;

    if (auto eventItemTmp = eventItem) {
        // set evItem's parentItem to null will invoke clearFocusInScope then focusIn surfaceItem
        // first clear eventItem to avoid forceActiveFocus on eventItem again
        this->eventItem = nullptr;
        eventItemTmp->setVisible(false);
        eventItemTmp->setParentItem(nullptr);
        eventItemTmp->setParent(nullptr);
        eventItemTmp->deleteLater();
    } else {
        eventItem = new EventItem(q_func());
        eventItem->setZ(qreal(WSurfaceItem::ZOrder::EventItem));
        updateEventItemGeometry();
    }

    Q_EMIT q_func()->eventItemChanged();
}

void WSurfaceItemPrivate::updateEventItemGeometry()
{
    Q_ASSERT(eventItem);
    Q_ASSERT(contentContainer);
    QQuickItemPrivate::get(eventItem)->anchors()->setFill(contentContainer);
    eventItem->setTransformOrigin(contentContainer->transformOrigin());
    eventItem->setScale(contentContainer->scale());
}

void WSurfaceItemPrivate::doResize(WSurfaceItem::ResizeMode mode)
{
    Q_ASSERT(mode != WSurfaceItem::ManualResize);
    Q_ASSERT(effectiveVisible);
    Q_ASSERT(surfaceState);
    Q_Q(WSurfaceItem);

    if (mode == WSurfaceItem::SizeFromSurface) {
        const QSizeF content = (surfaceState->contentGeometry.size() / surfaceSizeRatio) + paddingsSize();
        q->setSize(content);
    } else if (mode == WSurfaceItem::SizeToSurface) {
        const QSizeF newSize = (q->size() - paddingsSize()) * surfaceSizeRatio;
        const QSizeF oldSize = surfaceState->contentGeometry.size();

        resizeSurfaceToItemSize(newSize.toSize(), (newSize - oldSize).toSize());
    } else {
        qWarning() << "Invalid resize mode" << mode;
    }
}

qreal WSurfaceItemPrivate::getImplicitWidth() const
{
    const auto ps = paddingsSize();
    if (!surfaceState)
        return ps.width();

    return surfaceState->contentGeometry.width() + ps.width();
}

qreal WSurfaceItemPrivate::getImplicitHeight() const
{
    const auto ps = paddingsSize();
    if (!surfaceState)
        return ps.height();

    return surfaceState->contentGeometry.height() + ps.height();
}


WToplevelSurface *WSurfaceItem::shellSurface() const
{
    return d_func()->shellSurface;
}

bool WSurfaceItem::setShellSurface(WToplevelSurface *surface)
{
    Q_D(WSurfaceItem);
    if (d->shellSurface == surface)
        return false;

    if (d->shellSurface) {
        bool ok = d->shellSurface->safeDisconnect(this);
        Q_ASSERT(ok);
    }
    d->shellSurface = surface;
    setSurface(surface ? surface->surface() : nullptr);
    Q_EMIT this->shellSurfaceChanged();
    return true;
}

WAYLIB_SERVER_END_NAMESPACE

#include "wsurfaceitem.moc"
