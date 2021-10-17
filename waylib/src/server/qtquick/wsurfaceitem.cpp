/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wsurfaceitem.h"
#include "wsurface.h"
#include "wtexture.h"
#include "woutput.h"
#include "wseat.h"
#include "wcursor.h"
#include "wthreadutils.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

#define STATE_MOVE QStringLiteral("move")
#define STATE_RESIZE QStringLiteral("resize")

class WSGTextureProvider : public QSGTextureProvider
{
public:
    WSGTextureProvider(WSurfaceItemPrivate *item)
        : item(item) {
        dwtexture.reset(new WTexture(nullptr));
    }

public:
    QSGTexture *texture() const override;
    void updateTexture(); // in render thread

private:
    WSurfaceItemPrivate *item;
    QScopedPointer<WTexture> dwtexture;
};

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

    inline WOutput *output() const {
        return WOutput::fromScreen(QQuickWindowPrivate::get(window)->topLevelScreen);
    }
    inline qreal scaleFromSurface() const {
        Q_Q(const WSurfaceItem);
        return surface->size().width() / q->width();
    }

    Q_DECLARE_PUBLIC(WSurfaceItem)
    WSurface *surface = nullptr;
    WSGTextureProvider *textureProvider = nullptr;
};

QSGTexture *WSGTextureProvider::texture() const
{
    return dwtexture->toSGTexture();
}

void WSGTextureProvider::updateTexture()
{
    dwtexture->setHandle(item->surface->texture());
    Q_EMIT textureChanged();
    item->q_func()->update();
}

WSurfaceItem::WSurfaceItem(QQuickItem *parent)
    : QQuickItem(*new WSurfaceItemPrivate(), parent)
{
    setFlag(QQuickItem::ItemHasContents);
    setFlag(QQuickItem::ItemClipsChildrenToShape);
    setAcceptHoverEvents(true);
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

    return d->surface->inputRegionContains(d->scaleFromSurface(), mapToGlobal(localPos));
}

void WSurfaceItem::setSurface(WSurface *surface)
{
    // TODO
    Q_D(WSurfaceItem);

    if (d->surface == surface)
        return;

    if (d->surface) {
        d->surface->disconnect(this);
        d->surface->disconnect(d->textureProvider);
    }

    d->surface = surface;

    if (d->surface) {
        // in render thread
        surface->server()->threadUtil()->run(this, d->textureProvider,
                                             &WSGTextureProvider::updateTexture);

        // in render thread
        connect(d->surface, &WSurface::textureChanged,
                d->textureProvider, &WSGTextureProvider::updateTexture,
                Qt::DirectConnection);

        connect(d->surface, &WSurface::destroyed, this,
                &WSurfaceItem::releaseResources, Qt::DirectConnection);
    }

    update();
    Q_EMIT surfaceChanged(d->surface);
}

WSurface *WSurfaceItem::surface() const
{
    Q_D(const WSurfaceItem);
    return d->surface;
}

WOutput *WSurfaceItem::output() const
{
    Q_D(const WSurfaceItem);
    return d->output();
}

QSGNode *WSurfaceItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    Q_D(WSurfaceItem);

    if (!d->surface || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setTexture(d->textureProvider->texture());
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry(QPointF(0, 0), d->surface->bufferSize());
    node->setSourceRect(textureGeometry);

    const qreal scale = window()->effectiveDevicePixelRatio();
    const qreal scale_surface = d->scaleFromSurface();
    const QRectF targetGeometry(d->surface->toEffectivePos(scale, d->surface->textureOffset()),
                                textureGeometry.size() / d->surface->scale() / scale_surface);
    node->setRect(targetGeometry);
    node->setFiltering(QSGTexture::Linear);

    d->surface->notifyFrameDone();

    return node;
}

void WSurfaceItem::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(WSurfaceItem);

    // The Non-Toplevel windows not need intercept the input events, it's events is distribute
    // via WSet in it's parent surface.
    if (d->surface->parentSurface()) {
        return QQuickItem::hoverEnterEvent(event);
    }

    auto we = d->output()->currentInputEvent();
    if (Q_UNLIKELY(!we))
        return;

    event->accept();
    auto seat = we->data->seat;
    seat->notifyEnterSurface(d->surface, we);
}

void WSurfaceItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(WSurfaceItem);

    auto we = d->output()->currentInputEvent();
    if (Q_UNLIKELY(!we))
        return;

    auto seat = we->data->seat;

    if (seat->hoverSurface() != d->surface) {
        return QQuickItem::hoverLeaveEvent(event);
    }

    event->accept();
    seat->notifyLeaveSurface(d->surface, we);
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

void WSurfaceItem::invalidateSceneGraph()
{
    Q_D(WSurfaceItem);
    if (d->textureProvider)
        delete d->textureProvider;
    d->textureProvider = nullptr;
    d->surface = nullptr;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfaceitem.cpp"
