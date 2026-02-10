// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

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
        qCWarning(waylibBufferItem)
            << "WBufferItem::wTextureProvider can only be queried on the rendering thread of a WOutputRenderWindow";
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

QObject *WBufferItem::buffer() const
{
    W_DC(WBufferItem);
    return d->buffer.get();
}

void WBufferItem::setBuffer(QObject *buffer)
{
    W_D(WBufferItem);

    QW_NAMESPACE::qw_buffer *ibuffer = qobject_cast<QW_NAMESPACE::qw_buffer*>(buffer);
    if (buffer != nullptr && ibuffer == nullptr) {
        qCWarning(waylibBufferItem) << "Reject expects a qw_buffer; ignoring incompatible object" << buffer;
        return;
    }

    if (d->buffer.get() == ibuffer)
        return;

    // Validate buffer basic attributes before locking to avoid holding unusable buffers.
    if (ibuffer) {
        auto *h = ibuffer->handle();
        if (!h || h->width <= 0 || h->height <= 0) {
            qCWarning(waylibBufferItem) << "Reject buffer with invalid size or handle"
                                        << buffer << "w" << (h ? h->width : -1)
                                        << "h" << (h ? h->height : -1);
            return;
        }
    }

    if (ibuffer)
        ibuffer->lock();

    d->buffer.reset(ibuffer);

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
        int bufW = -1;
        int bufH = -1;
        if (auto *buf = d->buffer.get()) {
            if (auto *h = buf->handle()) {
                bufW = h->width;
                bufH = h->height;
            }
        }
        qCWarning(waylibBufferItem) << "texture missing or item size invalid"
                                    << "buffer" << d->buffer.get()
                                    << "bufW" << bufW
                                    << "bufH" << bufH
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
    // Only mark dirty if we have a valid window to avoid crashes during window destruction
    if (window()) {
        QQuickItemPrivate::get(this)->dirty(QQuickItemPrivate::Content);
    }
}

void WBufferItem::invalidateSceneGraph()
{
    W_D(WBufferItem);
    delete d->textureProvider; // safe on nullptr
    d->textureProvider = nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
