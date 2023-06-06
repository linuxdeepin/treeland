// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsurfaceitem.h"
#include "wsurface.h"
#include "wsurfacehandler.h"
#include "wtexture.h"
#include "wseat.h"
#include "wcursor.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

#define STATE_MOVE QStringLiteral("move")
#define STATE_RESIZE QStringLiteral("resize")

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

private:
    WSurfaceItem *item;
};

QPointF SurfaceHandler::position() const
{
    return item->position();
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

    void initForSurface();

    Q_DECLARE_PUBLIC(WSurfaceItem)
    QPointer<WSurface> surface;
    std::unique_ptr<SurfaceHandler> surfaceHandler;
    WSGTextureProvider *textureProvider = nullptr;
};

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
    QMetaObject::invokeMethod(item->q_func(), &QQuickItem::update, Qt::QueuedConnection);
}

WSurfaceItem::WSurfaceItem(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemPrivate(), parent)
{
    setFlag(QQuickItem::ItemHasContents);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
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

    if (Q_UNLIKELY(!d->surface))
        return false;

    return d->surface->inputRegionContains(localPos);
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
    update();

    Q_EMIT surfaceChanged();
}

QSGNode *WSurfaceItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    Q_D(WSurfaceItem);

    if (!d->surface || width() <= 0 || height() <= 0) {
        delete oldNode;

        if (d->surface) {
            d->surface->notifyFrameDone();
        }

        return nullptr;
    }

    auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        auto texture = d->textureProvider->texture();
        if (!texture)
            return node;

        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setTexture(texture);
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry(QPointF(0, 0), d->surface->bufferSize());
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(d->surface->textureOffset(), size());
    node->setRect(targetGeometry);
    node->setFiltering(QSGTexture::Linear);

    // TODO: Move to WOutputRenderWindowPrivate::doRender
    d->surface->notifyFrameDone();

    return node;
}

void WSurfaceItem::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(WSurfaceItem);

    auto inputDevice = WInputDevice::from(event->device());
    if (Q_UNLIKELY(!inputDevice))
        return;

    event->accept();
    inputDevice->seat()->notifyEnterSurface(d->surface);
}

void WSurfaceItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(WSurfaceItem);

    auto inputDevice = WInputDevice::from(event->device());
    if (Q_UNLIKELY(!inputDevice))
        return;

    auto seat = inputDevice->seat();

    if (seat->hoverSurface() != d->surface) {
        return QQuickItem::hoverLeaveEvent(event);
    }

    event->accept();
    seat->notifyLeaveSurface(d->surface);
}

void WSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    event->accept();
}

void WSurfaceItem::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(WSurfaceItem);

    QQuickItem::mouseReleaseEvent(event);
}

void WSurfaceItem::releaseResources()
{
    Q_D(WSurfaceItem);

    if (d->textureProvider) {
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
        window()->scheduleRenderJob(new WSurfaceCleanupJob(d->textureProvider),
                                    QQuickWindow::AfterRenderingStage);
        d->textureProvider = nullptr;
    }

    d->surface = nullptr;
    // Force to update the contents, avoid to render the invalid textures
    d->dirty(QQuickItemPrivate::Content);
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

    if (d->surface && d->componentComplete
        && newGeometry.size() != oldGeometry.size()) {
        d->surface->resize(newGeometry.size().toSize());
    }

    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

void WSurfaceItem::invalidateSceneGraph()
{
    Q_D(WSurfaceItem);
    if (d->textureProvider)
        delete d->textureProvider;
    d->textureProvider = nullptr;
    d->surface = nullptr;
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

    if (widthValid() && heightValid()) {
        surface->resize(q->size().toSize());
    } else {
        updateImplicitSize();
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfaceitem.cpp"
