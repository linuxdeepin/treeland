// Copyright (C) 2025
// SPDX-License-Identifier: Apache-2.0

#include "wbufferitem.h"

#include "woutputrenderwindow.h"
#include "wsgtextureprovider.h"

#include <qwbuffer.h>

#include <QQuickWindow>
#include <QSGImageNode>
#include <QThread>
#include <QLoggingCategory>
#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(waylibBufferItem, "waylib.server.qtquick.bufferitem", QtInfoMsg)

class Q_DECL_HIDDEN WBufferItemPrivate : public QQuickItemPrivate
{
public:
    explicit WBufferItemPrivate(WBufferItem *) {}

    void cleanTextureProvider()
    {
        if (textureProvider) {
            delete textureProvider;
            textureProvider = nullptr;
        }
    }

    W_DECLARE_PUBLIC(WBufferItem)
    mutable WSGTextureProvider *textureProvider = nullptr;
    std::unique_ptr<qw_buffer, qw_buffer::unlocker> buffer;
};


WBufferItem::WBufferItem(QQuickItem *parent)
    : QQuickItem(*new WBufferItemPrivate(this), parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

WBufferItem::~WBufferItem()
{
    W_D(WBufferItem);
    // `d->window` will become nullptr in ~QQuickItem; delete provider here.
    d->cleanTextureProvider();
}

bool WBufferItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *WBufferItem::textureProvider() const
{
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    return wTextureProvider();
}

WSGTextureProvider *WBufferItem::wTextureProvider() const
{
    W_DC(WBufferItem);

    auto w = qobject_cast<WOutputRenderWindow*>(d->window);
    if (!w || !d->sceneGraphRenderContext() || QThread::currentThread() != d->sceneGraphRenderContext()->thread()) {
        qWarning("WQuickCursor::textureProvider: can only be queried on the rendering thread of an WOutputRenderWindow");
        return nullptr;
    }

    if (!d->textureProvider) {
        d->textureProvider = new WSGTextureProvider(w);
        d->textureProvider->setSmooth(smooth());
        connect(this, &WBufferItem::smoothChanged,
                d->textureProvider, &WSGTextureProvider::setSmooth);
        d->textureProvider->setBuffer(d->buffer.get());
    }
    return d->textureProvider;
}

WOutputRenderWindow *WBufferItem::outputRenderWindow() const
{
    return qobject_cast<WOutputRenderWindow*>(window());
}

QW_NAMESPACE::qw_buffer *WBufferItem::buffer() const
{
    W_DC(WBufferItem);
    return d->buffer.get();
}

QVariant WBufferItem::bufferVariant() const
{
    W_DC(WBufferItem);
    return QVariant::fromValue(d->buffer.get());
}

void WBufferItem::setBufferVariant(const QVariant &buffer)
{
    setBuffer(buffer.value<QW_NAMESPACE::qw_buffer*>());
}

void WBufferItem::setBuffer(QW_NAMESPACE::qw_buffer *buffer)
{
    W_D(WBufferItem);

    qCDebug(waylibBufferItem) << "setBuffer" << buffer
                              << "w" << (buffer ? buffer->handle()->width : -1)
                              << "h" << (buffer ? buffer->handle()->height : -1)
                              << "locks" << (buffer ? buffer->handle()->n_locks : -1);

    if (d->buffer.get() == buffer)
        return;

    if (buffer)
        buffer->lock();

    d->buffer.reset(buffer);

    if (d->textureProvider)
        d->textureProvider->setBuffer(d->buffer.get());

    update();
    Q_EMIT bufferChanged();
}

void WBufferItem::componentComplete()
{
    QQuickItem::componentComplete();
}

QSGNode *WBufferItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    W_D(WBufferItem);

    auto tp = wTextureProvider();
    if (!tp) {
        qCWarning(waylibBufferItem) << "wTextureProvider() is nullptr";
        return nullptr;
    }

    // Refresh provider with the latest buffer.
    tp->setBuffer(d->buffer.get());

    if (!tp->texture() || width() <= 0 || height() <= 0) {
        qCWarning(waylibBufferItem) << "texture missing or item size invalid"
                                    << "buffer" << d->buffer.get()
                                    << "bufW" << (d->buffer.get() ? d->buffer.get()->handle()->width : -1)
                                    << "bufH" << (d->buffer.get() ? d->buffer.get()->handle()->height : -1)
                                    << "itemW" << width() << "itemH" << height();
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGImageNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
    }

    auto texture = tp->texture();
    qCDebug(waylibBufferItem) << "updatePaintNode" << "texSize" << texture->textureSize()
                              << "itemSize" << size();
    node->setTexture(texture);
    node->setSourceRect(QRectF(QPointF(), texture->textureSize()));
    node->setRect(QRectF(QPointF(), size()));
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);

    return node;
}

void WBufferItem::releaseResources()
{
    W_D(WBufferItem);

    d->cleanTextureProvider();
    // Keep last buffer cached; just force content dirty to avoid stale nodes.
    QQuickItemPrivate::get(this)->dirty(QQuickItemPrivate::Content);
}

void WBufferItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    Q_UNUSED(change);
    Q_UNUSED(data);
}

void WBufferItem::invalidateSceneGraph()
{
    W_D(WBufferItem);
    if (d->textureProvider)
        delete d->textureProvider;
    d->textureProvider = nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
