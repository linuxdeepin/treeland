// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wsurface.h"
#include "wseat.h"
#include "wcursor.h"
#include "woutput.h"
#include "woutputviewport.h"
#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"

#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwtexture.h>
#include <qwbuffer.h>
#include <qwrenderer.h>
#include <qwbox.h>
#include <qwalphamodifierv1.h>

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRenderNode>
#include <QQueue>
#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(waylibSurface, "waylib.server.surface", QtInfoMsg)

class Q_DECL_HIDDEN SubsurfaceContainer : public QQuickItem
{
    Q_OBJECT
public:
    explicit SubsurfaceContainer(WSurfaceItem *mainSurface)
        : QQuickItem(mainSurface)
    {
    }

    bool isEmpty() const {
        return m_subsurfaces.empty();
    }

    void deleteAfterEmpty() {
        m_deleteAfterEmpty = true;
    }
Q_SIGNALS:
    void subsurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WSurfaceItem *item);

protected:
    void itemChange(ItemChange change, const ItemChangeData &data) override
    {
        switch (change) {
        case QQuickItem::ItemChildAddedChange: {
            auto surfaceItem = static_cast<WSurfaceItem *>(data.item);
            Q_ASSERT(surfaceItem); // TODO: check isSubsurface, may segmentation fault now.
            m_subsurfaces.append(surfaceItem);
            m_deleteAfterEmpty = false;
            break;
        }
        case QQuickItem::ItemChildRemovedChange: {
            auto surfaceItem = static_cast<WSurfaceItem *>(data.item);
            Q_ASSERT(surfaceItem && m_subsurfaces.contains(surfaceItem));
            m_subsurfaces.removeOne(surfaceItem);
            Q_EMIT subsurfaceRemoved(surfaceItem);
            if (m_deleteAfterEmpty)
                deleteLater();
            break;
        }
        default:
            break;
        }
    }

private:
    QList<WSurfaceItem *> m_subsurfaces;
    bool m_deleteAfterEmpty {false};
};

class Q_DECL_HIDDEN EventItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit EventItem(WSurfaceItem *parent)
        : QQuickItem(parent) {
        setAcceptHoverEvents(true);
        setAcceptTouchEvents(true);
        setAcceptedMouseButtons(Qt::AllButtons);
        setFlag(QQuickItem::ItemClipsChildrenToShape, true);
        setCursor(WCursor::toQCursor(WGlobal::CursorShape::ClientResource));
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

class Q_DECL_HIDDEN WSurfaceItemContentPrivate: public QQuickItemPrivate
{
public:
    WSurfaceItemContentPrivate([[maybe_unused]] WSurfaceItemContent *qq){}

    ~WSurfaceItemContentPrivate() {
    }

    void cleanTextureProvider();

    void invalidate() {
        W_Q(WSurfaceItemContent);
        if (surface) {
            surface->safeDisconnect(q);
            if (textureProvider) {
                surface->safeDisconnect(textureProvider);
            }
            surface = nullptr;
        }

        if (frameDoneConnection)
            QObject::disconnect(frameDoneConnection);

        Q_ASSERT(!updateTextureConnection);

        if (dontCacheLastBuffer) {
            buffer.reset();
            cleanTextureProvider();
            q->update();
        }
    }

    void init() {
        W_Q(WSurfaceItemContent);

        surface->safeConnect(&WSurface::aboutToBeInvalidated, q, [this] {
            invalidate();
        });
        surface->safeConnect(&qw_surface::notify_commit, q, [this] {
            updateSurfaceState();
        });

        Q_ASSERT(!updateTextureConnection);
        updateTextureConnection = surface->safeConnect(&WSurface::commit,
                                                       q, [q, this] (quint32 committedState) {
            const bool bufferChanged = committedState & WLR_SURFACE_STATE_BUFFER;

            if (bufferChanged) {
                if (!live) {
                    pendingBuffer.reset(surface->buffer());
                    if (Q_LIKELY(pendingBuffer))
                        pendingBuffer->lock();
                } else {
                    buffer.reset(surface->buffer());
                    // lock buffer to ensure the WSurfaceItem can keep the last frame after WSurface destroyed.
                    if (Q_LIKELY(buffer))
                        buffer->lock();

                    q->update();
                }
            }

            if (Q_LIKELY((q->isVisible() || lastRendered) && live))
                surface->scheduleFrameIfNeeded();
        });

        updateFrameDoneConnection();
        updateSurfaceState();
        rendered = true;
        lastRendered = true;
    }

    void updateFrameDoneConnection() {
        W_Q(WSurfaceItemContent);

        if (frameDoneConnection)
            QObject::disconnect(frameDoneConnection);
        if (!q->window()) // maybe null due to item not fully initialized
            return;

        auto rw = q->outputRenderWindow();
        if (Q_LIKELY(rw)) {
            // wayland protocol job should not run in rendering thread, so set context qobject to contentItem
            frameDoneConnection = QObject::connect(rw, &WOutputRenderWindow::renderEnd,
                                                   q, [this, q] (const QList<QPointer<WOutput>> committedOutputs) {
                                                       lastRendered = rendered;
                                                       if (Q_LIKELY((rendered || q->isVisible()) && live)
                                                           && committedOutputs.contains(surface->framePacingOutput())) {
                                                           surface->notifyFrameDone();
                                                           rendered = false;
                                                       }
                                                   }); // if signal is emitted from seperated rendering thread, default QueuedConnection is used
        } else {
            qCFatal(waylibSurface) << "Needs a WOutputRenderWindow to render the WSurfaceItemContent, "
                                      "but the current window is:" << q->window();
        }
    }

    void updateSurfaceState() {
        if (!surface)
            return;

        qw_fbox tmp;
        surface->handle()->get_buffer_source_box(tmp);
        auto newBufferSourceBox = tmp.toQRectF();
        std::swap(newBufferSourceBox, bufferSourceBox);

        W_Q(WSurfaceItemContent);

        const wlr_alpha_modifier_surface_v1_state *alphaModifierState =
            qw_alpha_modifier_v1::get_surface_state(surface->handle()->handle());
        if (alphaModifierState)
            setAlphaModifier(alphaModifierState->multiplier);

        const auto bOffset = surface->bufferOffset();
        if (bOffset != bufferOffset) {
            bufferOffset = surface->bufferOffset();
            Q_EMIT q->bufferOffsetChanged();
        }

        if (bufferSourceBox != newBufferSourceBox) {
            Q_EMIT q->bufferSourceRectChanged();
        }

        const auto s = surface->size();
        q->setImplicitSize(s.width(), s.height());
    }

    inline void swapBufferIfNeeded() {
        if (pendingBuffer) {
            buffer.reset(pendingBuffer.release());
        }
    }

    inline void setDevicePixelRatio(qreal dpr) {
        if (dpr == devicePixelRatio)
            return;
        devicePixelRatio = dpr;
        Q_EMIT q_func()->devicePixelRatioChanged();
        Q_EMIT q_func()->bufferSourceRectChanged();
    }

    inline void setAlphaModifier(qreal alpha) {
        if (qFuzzyCompare(alphaModifier, alpha))
            return;

        alphaModifier = alpha;

        W_Q(WSurfaceItemContent);

        Q_EMIT q->alphaModifierChanged();
    }

    W_DECLARE_PUBLIC(WSurfaceItemContent)
    QPointer<WSurface> surface;
    QRectF bufferSourceBox;
    QPoint bufferOffset;
    qreal devicePixelRatio = 1.0;
    qreal alphaModifier = 1.0;

    QMetaObject::Connection frameDoneConnection;
    mutable WSGTextureProvider *textureProvider = nullptr;
    std::unique_ptr<qw_buffer, qw_buffer::unlocker> buffer;
    std::unique_ptr<qw_buffer, qw_buffer::unlocker> pendingBuffer;
    mutable QMetaObject::Connection updateTextureConnection;
    bool dontCacheLastBuffer = false;
    bool live = true;
    bool ignoreBufferOffset = false;
    bool lastRendered = false;
    QAtomicInteger<bool> rendered = false;
};


WSurfaceItemContent::WSurfaceItemContent(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemContentPrivate(this), parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

WSurfaceItemContent::~WSurfaceItemContent()
{
    W_D(WSurfaceItemContent);
    if (d->updateTextureConnection) {
        Q_ASSERT(d->surface);
        d->surface->safeDisconnect(d->updateTextureConnection);
    }

    if (d->frameDoneConnection)
        QObject::disconnect(d->frameDoneConnection);

    //`d->window` will become nullptr in ~QQuickItem
    // Don't move this to private class
    d->cleanTextureProvider();
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

    if (!d->surface) {
        d->invalidate();
    } else {
        const auto s = surface->size();
        setImplicitSize(s.width(), s.height());
    }

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

    return wTextureProvider();
}

WSGTextureProvider *WSurfaceItemContent::wTextureProvider() const
{
    W_DC(WSurfaceItemContent);

    auto w = qobject_cast<WOutputRenderWindow*>(d->window);
    if (!w || !d->sceneGraphRenderContext() || QThread::currentThread() != d->sceneGraphRenderContext()->thread()) {
        qWarning("WQuickCursor::textureProvider: can only be queried on the rendering thread of an WOutputRenderWindow");
        return nullptr;
    }

    if (!d->textureProvider) {
        d->textureProvider = new WSGTextureProvider(w);
        d->textureProvider->setSmooth(smooth());
        connect(this, &WSurfaceItemContent::smoothChanged,
                d->textureProvider, &WSGTextureProvider::setSmooth);

        if (d->surface) {
            if (auto texture = d->surface->handle()->get_texture()) {
                d->textureProvider->setTexture(qw_texture::from(texture), d->buffer.get());
            } else {
                d->textureProvider->setBuffer(d->buffer.get());
            }
        }
    }
    return d->textureProvider;
}

WOutputRenderWindow *WSurfaceItemContent::outputRenderWindow() const
{
    return qobject_cast<WOutputRenderWindow*>(window());
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
    if (live) {
        d->swapBufferIfNeeded();
        update();
    }
    Q_EMIT liveChanged();
}

QPoint WSurfaceItemContent::bufferOffset() const
{
    W_DC(WSurfaceItemContent);
    return d->bufferOffset;
}

bool WSurfaceItemContent::ignoreBufferOffset() const
{
    W_DC(WSurfaceItemContent);
    return d->ignoreBufferOffset;
}

void WSurfaceItemContent::setIgnoreBufferOffset(bool newIgnoreBufferOffset)
{
    W_D(WSurfaceItemContent);
    if (d->ignoreBufferOffset == newIgnoreBufferOffset)
        return;
    d->ignoreBufferOffset = newIgnoreBufferOffset;
    Q_EMIT ignoreBufferOffsetChanged();
}


QRectF WSurfaceItemContent::bufferSourceRect() const
{
    W_DC(WSurfaceItemContent);
    return QRectF(d->bufferSourceBox.topLeft() / d->devicePixelRatio,
                  d->bufferSourceBox.size() / d->devicePixelRatio);
}

qreal WSurfaceItemContent::devicePixelRatio() const
{
    W_DC(WSurfaceItemContent);
    return d->devicePixelRatio;
}

void WSurfaceItemContent::componentComplete()
{
    QQuickItem::componentComplete();

    W_D(WSurfaceItemContent);
    if (d->surface)
        d->init();
}

qreal WSurfaceItemContent::alphaModifier() const
{
    W_DC(WSurfaceItemContent);
    return d->alphaModifier;
}

class Q_DECL_HIDDEN WSGRenderFootprintNode: public QSGRenderNode
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
            m_owner->d_func()->rendered = true;
    }

    RenderingFlags flags() const override
    {
        return QSGRenderNode::NoExternalRendering | QSGRenderNode::BoundedRectRendering;
    }

    QPointer<WSurfaceItemContent> m_owner;
};

QSGNode *WSurfaceItemContent::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    W_D(WSurfaceItemContent);

    auto tp = wTextureProvider();
    if (d->live || !tp->texture()) {
        auto texture = d->surface ? d->surface->handle()->get_texture() : nullptr;
        if (texture) {
            tp->setTexture(qw_texture::from(texture), d->buffer.get());
        } else {
            tp->setBuffer(d->buffer.get());
        }
    }

    if (!tp->texture() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGImageNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
        QSGNode *fpnode = new WSGRenderFootprintNode(this);
        node->appendChildNode(fpnode);
    }

    auto texture = tp->texture();
    node->setTexture(texture);
    const QRectF textureGeometry = d->bufferSourceBox;
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(d->ignoreBufferOffset ? QPointF() : d->bufferOffset, size());
    node->setRect(targetGeometry);
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);

    return node;
}

void WSurfaceItemContent::releaseResources()
{
    W_D(WSurfaceItemContent);

    d->cleanTextureProvider();
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
        d->setDevicePixelRatio(data.window ? data.window->effectiveDevicePixelRatio() : 1.0);
    } else if (change == QQuickItem::ItemDevicePixelRatioHasChanged) {
        d->setDevicePixelRatio(data.realValue);
    }
}

void WSurfaceItemContent::invalidateSceneGraph()
{
    W_D(WSurfaceItemContent);
    if (d->textureProvider)
        delete d->textureProvider;
    d->textureProvider = nullptr;
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

QRectF WSurfaceItem::boundingRect() const
{
    W_DC(WSurfaceItem);
    return d->boundingRect;
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

    if (auto content = d->getItemContent()) {
        content->setCacheLastBuffer(!newFlags.testFlag(DontCacheLastBuffer));
        content->setLive(!newFlags.testFlag(NonLive));
    }

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
    setImplicitWidth(d->calculateImplicitWidth());
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
    d->delegateIsDirty = true;
    if (d->componentComplete)
        d->initForDelegate();

    if (flags() & DelegateForSubsurface) {
        for (auto sub : std::as_const(d->subsurfaces))
            sub->setDelegate(newDelegate);
    }

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
    setImplicitWidth(d->calculateImplicitWidth());
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
    setImplicitHeight(d->calculateImplicitHeight());
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
    setImplicitHeight(d->calculateImplicitHeight());
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
        if (d->contentContainer)
            d->contentContainer->setSize(d->contentContainer->size() +
                                         (newGeometry.size() - oldGeometry.size()) * d->surfaceSizeRatio);
    }

    d->updateBoundingRect();
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
                d->updateBoundingRect();
            }
        }

        Q_EMIT effectiveVisibleChanged();
    }
}

void WSurfaceItem::focusInEvent(QFocusEvent *event)
{
    QQuickItem::focusInEvent(event);

    // Q_D(WSurfaceItem);
    // if (d->eventItem)
    //     d->eventItem->forceActiveFocus(event->reason());
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
    QRectF tmp(0, 0, newSize.width(), newSize.height());
    tmp -= d->paddings;
    // See surfaceSizeRatio, the content item maybe has been scaled.
    const QSize mappedSize = mapRectToItem(d->contentContainer, tmp).size().toSize();
    QSize clipedSize;
    if (!d->shellSurface->checkNewSize(mappedSize, &clipedSize))
        return doResizeSurface(clipedSize);
    else
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

    if (d->contentContainer) {
        d->contentContainer->setTransformOrigin(QQuickItem::TopLeft);
        d->contentContainer->setScale(1.0 / d->surfaceSizeRatio);
    }

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

    d->surfaceState->contentGeometry = getContentGeometry();
    d->surfaceState->contentSize = getContentSize();

    setImplicitSize(d->calculateImplicitWidth(),
                    d->calculateImplicitHeight());

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

    surface->safeConnect(&WSurface::aboutToBeInvalidated, q,
                         &WSurfaceItem::releaseResources, Qt::DirectConnection);
    surface->safeConnect(&WSurface::hasSubsurfaceChanged, q, [this]{ onHasSubsurfaceChanged(); });
    surface->safeConnect(&qw_surface::notify_commit, q, &WSurfaceItem::onSurfaceCommit);

    onHasSubsurfaceChanged();
    updateEventItem(false);
    q->onSurfaceCommit();
}

void WSurfaceItemPrivate::initForDelegate()
{
    Q_Q(WSurfaceItem);

    std::unique_ptr<QQuickItem> newContentContainer;

    if (!delegate) {
        if (getItemContent()) {
            Q_ASSERT(!delegateIsDirty);
            return;
        }

        delegateIsDirty = false;
        auto contentItem = new WSurfaceItemContent(q);
        if (surface)
            contentItem->setSurface(surface);
        contentItem->setCacheLastBuffer(!surfaceFlags.testFlag(WSurfaceItem::DontCacheLastBuffer));
        contentItem->setSmooth(q->smooth());
        contentItem->setLive(!q->flags().testFlag(WSurfaceItem::NonLive));
        QObject::connect(q, &WSurfaceItem::smoothChanged, contentItem, &WSurfaceItemContent::setSmooth);
        newContentContainer.reset(contentItem);
    } else if (delegateIsDirty) {
        auto obj = delegate->createWithInitialProperties({{"surface", QVariant::fromValue(q)}}, qmlContext(q));
        if (!obj) {
            qWarning() << "Failed on create surface item from delegate, error mssage:"
                       << delegate->errorString();
            return;
        }

        delegateIsDirty = false;
        auto contentItem = qobject_cast<QQuickItem*>(obj);
        if (!contentItem)
            qFatal() << "SurfaceItem's delegate must is Item";

        newContentContainer.reset(new QQuickItem(q));
        QQmlEngine::setObjectOwnership(contentItem, QQmlEngine::CppOwnership);
        contentItem->setParent(newContentContainer.get());
        contentItem->setParentItem(newContentContainer.get());
    }

    if (!newContentContainer)
        return;

    newContentContainer->setZ(qreal(WSurfaceItem::ZOrder::ContentItem));

    if (contentContainer) {
        newContentContainer->setPosition(contentContainer->position());
        newContentContainer->setSize(contentContainer->size());
        newContentContainer->setTransformOrigin(contentContainer->transformOrigin());
        newContentContainer->setScale(contentContainer->scale());

        contentContainer->disconnect(q);
        contentContainer->deleteLater();
    } else {
        newContentContainer->setTransformOrigin(QQuickItem::TransformOrigin::TopLeft);
        newContentContainer->setScale(1.0 / surfaceSizeRatio);
    }
    contentContainer = newContentContainer.release();

    updateEventItem(false);
    updateBoundingRect();
    if (eventItem)
        updateEventItemGeometry();

    Q_EMIT q->contentItemChanged();
}

void WSurfaceItemPrivate::onHasSubsurfaceChanged()
{
    auto qw_surface = surface->handle();
    Q_ASSERT(qw_surface);
    if (surface->hasSubsurface())
        updateSubsurfaceItem();
}

void WSurfaceItemPrivate::updateSubsurfaceItem()
{
    auto surface = this->surface->handle()->handle();
    Q_ASSERT(surface);
    Q_ASSERT(contentContainer);
    updateSubsurfaceContainers();
    auto updateForContainer = [this](wl_list *subsurfaceList, QQuickItem *container) {
        wlr_subsurface *subsurface;
        QQuickItem *prev = nullptr;
        wl_list_for_each(subsurface, subsurfaceList, current.link) {
            WSurface *surface = WSurface::fromHandle(subsurface->surface);
            if (!surface)
                continue;
            WSurfaceItem *item = ensureSubsurfaceItem(surface, container);
            item->setSurfaceSizeRatio(surfaceSizeRatio);
            Q_ASSERT(item->parentItem() == container);
            if (prev) {
                Q_ASSERT(prev->parentItem() == item->parentItem());
                item->stackAfter(prev);
            }
            prev = item;
            const QPointF pos = contentContainer->position() + QPointF(subsurface->current.x, subsurface->current.y) / surfaceSizeRatio;
            item->setPosition(pos);
        }
    };
    updateForContainer(&surface->current.subsurfaces_below, belowSubsurfaceContainer);
    updateForContainer(&surface->current.subsurfaces_above, aboveSubsurfaceContainer);
    updateBoundingRect();
}

void WSurfaceItemPrivate::onPaddingsChanged()
{
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
    updateBoundingRect();
}

WSurfaceItem *WSurfaceItemPrivate::ensureSubsurfaceItem(WSurface *subsurfaceSurface, QQuickItem *parent)
{
    for (int i = 0; i < subsurfaces.count(); ++i) {
        auto surfaceItem = subsurfaces.at(i);
        WSurface *surface = surfaceItem->d_func()->surface.get();

        if (surface && surface == subsurfaceSurface) {
            if (surfaceItem->parent() == parent) {
                return surfaceItem;
            } else {
                subsurfaces.removeOne(surfaceItem);
                break;
            }
        }
    }

    Q_Q(WSurfaceItem);
    Q_ASSERT(subsurfaceSurface);
    auto surfaceItem = new WSurfaceItem(parent);
    // Delay destroy WSurfaceItem, because if the cause of destroy is because the parent
    // surface destroy, and the parent WSurfaceItem::cacheLastBuffer maybe enabled,
    // will disable this connection at parent WSurfaceItem::releaseResources to save the
    // parent WSurface last frame, the last frame contents should include its subsurfaces's
    // contents.
    // AutoDestroy: Connect to this(parent)'s lambda since the autodestroy is managed by parent,
    // avoids disconnected with all slots on subsurfaceItem in subsurface's releaseResources
    QObject::connect(subsurfaceSurface, &WSurface::destroyed, q, [this,surfaceItem] {
        subsurfaces.removeOne(surfaceItem);
        surfaceItem->deleteLater();
    }, Qt::QueuedConnection);
    surfaceItem->setDelegate(delegate);
    surfaceItem->setFlags(surfaceFlags);
    surfaceItem->setSurface(subsurfaceSurface);
    surfaceItem->setSmooth(q->smooth());
    QObject::connect(q, &WSurfaceItem::smoothChanged, surfaceItem, &WSurfaceItem::setSmooth);
    QObject::connect(surfaceItem, &WSurfaceItem::boundingRectChanged, q, [this] {
        updateBoundingRect();
    });
    // remove list element in WSurfaceItem::itemChange
    subsurfaces.append(surfaceItem);
    Q_EMIT q->subsurfaceAdded(surfaceItem);

    return surfaceItem;
}

void WSurfaceItemPrivate::updateSubsurfaceContainers()
{
    Q_Q(WSurfaceItem);
    if (wl_list_empty(&surface->handle()->handle()->current.subsurfaces_below) && belowSubsurfaceContainer) {
        if (belowSubsurfaceContainer->isEmpty()) {
            delete belowSubsurfaceContainer;
        } else {
            belowSubsurfaceContainer->deleteAfterEmpty();
        }
    } else if (!wl_list_empty(&surface->handle()->handle()->current.subsurfaces_below) && !belowSubsurfaceContainer) {
        belowSubsurfaceContainer = new SubsurfaceContainer(q);
        belowSubsurfaceContainer->setZ(static_cast<qreal>(WSurfaceItem::ZOrder::BelowSubsurface));
        belowSubsurfaceContainer->setVisible(subsurfacesVisible);
        QQuickItemPrivate::get(belowSubsurfaceContainer)->anchors()->setFill(q);
        QObject::connect(belowSubsurfaceContainer, &SubsurfaceContainer::subsurfaceRemoved, q, [this, q](WSurfaceItem *item) {
            updateBoundingRect();
            Q_EMIT q->subsurfaceRemoved(item);
        });
    }
    if (wl_list_empty(&surface->handle()->handle()->current.subsurfaces_above) && aboveSubsurfaceContainer) {
        if (aboveSubsurfaceContainer->isEmpty()) {
            delete aboveSubsurfaceContainer;
        } else {
            aboveSubsurfaceContainer->deleteAfterEmpty();
        }
    } else if (!wl_list_empty(&surface->handle()->handle()->current.subsurfaces_above)  && !aboveSubsurfaceContainer) {
        aboveSubsurfaceContainer = new SubsurfaceContainer(q);
        aboveSubsurfaceContainer->setZ(static_cast<qreal>(WSurfaceItem::ZOrder::AboveSubsurface));
        aboveSubsurfaceContainer->setVisible(subsurfacesVisible);
        QQuickItemPrivate::get(aboveSubsurfaceContainer)->anchors()->setFill(q);
        QObject::connect(aboveSubsurfaceContainer, &SubsurfaceContainer::subsurfaceRemoved, q, [this, q](WSurfaceItem *item) {
            updateBoundingRect();
            Q_EMIT q->subsurfaceRemoved(item);
        });
    }
}

void WSurfaceItemPrivate::resizeSurfaceToItemSize(const QSize &itemSize, const QSize &sizeDiff)
{
    Q_Q(WSurfaceItem);

    Q_ASSERT_X(itemSize == ((q->size() - paddingsSize()) * surfaceSizeRatio).toSize(), "WSurfaceItem",
               "The function only using for reisze wl_surface's size to the WSurfaceItem's current size");

    if (!surface) {
        contentContainer->setSize(contentContainer->size() + sizeDiff);
        updateBoundingRect();
        return;
    }

    if (q->resizeSurface(itemSize)) {
        contentContainer->setSize(contentContainer->size() + sizeDiff);
        beforeRequestResizeSurfaceStateSeq = surface->handle()->handle()->pending.seq;
        updateBoundingRect();
    }
}

void WSurfaceItemPrivate::updateEventItem(bool forceDestroy)
{
    const bool needsEventItem = contentContainer && !forceDestroy
                                && !surfaceFlags.testFlag(WSurfaceItem::RejectEvent);
    if (bool(eventItem) == needsEventItem)
        return;

    if (auto eventItemTmp = eventItem) {
        // maybe trigger QQuickDeliveryAgentPrivate::clearFocusInScope in later,
        // first clear eventItem=nullptr to avoid forceActiveFocus on eventItem again.
        this->eventItem = nullptr;
        eventItemTmp->setVisible(false);
        eventItemTmp->setParent(nullptr);
        eventItemTmp->deleteLater();
    } else {
        eventItem = new EventItem(q_func());
        eventItem->setZ(qreal(WSurfaceItem::ZOrder::EventItem));
        eventItem->setFocus(true);
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

qreal WSurfaceItemPrivate::calculateImplicitWidth() const
{
    const auto ps = paddingsSize();
    if (!surfaceState)
        return ps.width();

    return surfaceState->contentGeometry.width() / surfaceSizeRatio + ps.width();
}

qreal WSurfaceItemPrivate::calculateImplicitHeight() const
{
    const auto ps = paddingsSize();
    if (!surfaceState)
        return ps.height();

    return surfaceState->contentGeometry.height() / surfaceSizeRatio + ps.height();
}

QRectF WSurfaceItemPrivate::calculateBoundingRect() const
{
    W_QC(WSurfaceItem);
    QRectF rect = QRectF(0, 0, q->width(), q->height());

    if (contentContainer)
        rect |= q->mapFromItem(contentContainer, contentContainer->boundingRect());

    for (auto sub : std::as_const(subsurfaces))
        rect |= sub->boundingRect().translated(sub->position());

    return rect;
}

void WSurfaceItemPrivate::updateBoundingRect()
{
    auto newBoundingRect = calculateBoundingRect();
    if (newBoundingRect == boundingRect)
        return;
    boundingRect = newBoundingRect;

    W_Q(WSurfaceItem);
    Q_EMIT q->boundingRectChanged();
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

void WSurfaceItemContentPrivate::cleanTextureProvider()
{
    if (textureProvider) {
        // needs check window, because maybe this item's window always is nullptr,
        // so not call WSurfaceItemContent::releaseResources before destroy.
        if (window) {
            class Q_DECL_HIDDEN WSurfaceItemContentCleanupJob : public QRunnable
            {
            public:
                WSurfaceItemContentCleanupJob(QObject *object) : m_object(object) { }
                void run() override {
                    delete m_object;
                }
                QObject *m_object;
            };

                   // Delay clean the textures on the next render after.
            window->scheduleRenderJob(new WSurfaceItemContentCleanupJob(textureProvider),
                                      QQuickWindow::AfterRenderingStage);
        } else {
            delete textureProvider;
        }

        textureProvider = nullptr;
    }
}

bool WSurfaceItem::subsurfacesVisible() const
{
    Q_D(const WSurfaceItem);
    return d->subsurfacesVisible;
}

void WSurfaceItem::setSubsurfacesVisible(bool newSubsurfacesVisible)
{
    Q_D(WSurfaceItem);
    if (d->subsurfacesVisible == newSubsurfacesVisible)
        return;
    d->subsurfacesVisible = newSubsurfacesVisible;
    if (d->belowSubsurfaceContainer)
        d->belowSubsurfaceContainer->setVisible(d->subsurfacesVisible);
    if (d->aboveSubsurfaceContainer)
        d->aboveSubsurfaceContainer->setVisible(d->subsurfacesVisible);
    Q_EMIT subsurfacesVisibleChanged();
}

WSurfaceItemContent *WSurfaceItem::findItemContent() const
{
    QQueue<QQuickItem *> q;
    q.enqueue(const_cast<WSurfaceItem*>(this));
    while (!q.empty()) {
        auto node = q.dequeue();
        if (auto content = qobject_cast<WSurfaceItemContent *>(node))
            return content;
        for (auto child : node->childItems()) {
            q.enqueue(child);
        }
    }
    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE

#include "wsurfaceitem.moc"
