// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickcursor.h"
#include "woutputrenderwindow.h"
#include "woutputitem.h"
#include "wcursorimage.h"
#include "wbuffertextureprovider.h"
#include "wimagebuffer.h"
#include "wtexture.h"
#include "wseat.h"
#include "wsurfaceitem.h"

#include <qwxcursormanager.h>
#include <qwbuffer.h>
#include <qwtexture.h>
#include <qwcompositor.h>

#include <QSGImageNode>
#include <private/qquickitem_p.h>
#include <private/qsgplaintexture_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN CursorTextureProvider : public WBufferTextureProvider
{
public:
    CursorTextureProvider() {
        m_texture.setOwnsTexture(false);
    }

    void setImage(const QImage &image, QQuickWindow *window) {
        WOutputRenderWindow* rw = qobject_cast<WOutputRenderWindow*>(window);
        Q_ASSERT(rw);

        if (image.isNull() || !rw->renderer()) {
            resetBuffer();
            return;
        }

        // WImageBufferImpl destroy following qw_buffer
        auto buffer = qw_buffer::create(new WImageBufferImpl(image),
                                       image.width(), image.height());
        this->buffer.reset(buffer);
        m_qwTexture.reset(qw_texture::from_buffer(*rw->renderer(), *buffer));
        WTexture::makeTexture(m_qwTexture.get(), &m_texture, window);

        Q_EMIT textureChanged();
    }

    void setProxy(WBufferTextureProvider *proxy) {
        if (this->proxy == proxy)
            return;

        if (this->proxy) {
            bool ok = this->proxy->disconnect(this);
            Q_ASSERT(ok);
        }

        this->proxy = proxy;

        if (this->proxy) {
            connect(proxy, &WBufferTextureProvider::textureChanged,
                    this, &CursorTextureProvider::textureChanged);
        }

        Q_EMIT textureChanged();
    }

    void resetBuffer() {
        if (buffer) {
            this->buffer.reset();
            this->m_qwTexture.reset();
            Q_EMIT textureChanged();
        }
    }
    void reset() {
        resetBuffer();
        setProxy(nullptr);
    }

    QSGTexture *texture() const override {
        if (proxy)
            return proxy->texture();
        return buffer ? const_cast<QSGPlainTexture*>(&m_texture) : nullptr;
    }
    qw_texture *qwTexture() const override {
        if (proxy)
            return proxy->qwTexture();
        return m_qwTexture.get();
    }
    qw_buffer *qwBuffer() const override {
        if (proxy)
            return proxy->qwBuffer();
        return buffer.get();
    }

    std::unique_ptr<qw_buffer, qw_buffer::droper> buffer;
    std::unique_ptr<qw_texture> m_qwTexture;
    QSGPlainTexture m_texture;

    QPointer<WBufferTextureProvider> proxy;
};

WQuickCursorAttached::WQuickCursorAttached(QQuickItem *parent)
    : QObject(parent)
{

}

QQuickItem *WQuickCursorAttached::parent() const
{
    return qobject_cast<QQuickItem*>(QObject::parent());
}

WGlobal::CursorShape WQuickCursorAttached::shape() const
{
    return static_cast<WGlobal::CursorShape>(parent()->cursor().shape());
}

void WQuickCursorAttached::setShape(WGlobal::CursorShape shape)
{
    if (this->shape() == shape)
        return;
    parent()->setCursor(WCursor::toQCursor(shape));
    Q_EMIT shapeChanged();
}

class Q_DECL_HIDDEN WQuickCursorPrivate : public QQuickItemPrivate
{
public:
    WQuickCursorPrivate(WQuickCursor *qq);
    ~WQuickCursorPrivate() {
    }

    inline static WQuickCursorPrivate *get(WQuickCursor *qq) {
        return qq->d_func();
    }

    CursorTextureProvider *tp() const;
    void cleanTextureProvider();

    void invalidate() {
        QObject::disconnect(updateTextureConnection);
        cleanTextureProvider();
    }

    void setSurface(WSurface *surface);
    void enterOutput(WOutput *output);
    void leaveOutput(WOutput *output);
    void updateXCursorManager();
    void updateTexture();
    void updateCursor();
    void updateImplicitSize();
    void setHotSpot(const QPoint &newHotSpot);

    inline quint32 getCursorSize() const {
        return qMax(cursorSize.width(), cursorSize.height());
    }

    Q_DECLARE_PUBLIC(WQuickCursor)

    mutable CursorTextureProvider *textureProvider = nullptr;
    mutable QMetaObject::Connection updateTextureConnection;

    WCursor *cursor = nullptr;
    QPointer<WOutput> output;
    WCursorImage *cursorImage = nullptr;

    WSurfaceItemContent *cursorSurfaceItem = nullptr;

    QString xcursorThemeName;
    QSize cursorSize = QSize(24, 24);
    QPoint hotSpot;
};

void WQuickCursorPrivate::setHotSpot(const QPoint &newHotSpot)
{
    if (hotSpot == newHotSpot)
        return;
    hotSpot = newHotSpot;

    W_Q(WQuickCursor);
    Q_EMIT q->hotSpotChanged();
}

WQuickCursorPrivate::WQuickCursorPrivate(WQuickCursor *qq)
    : QQuickItemPrivate()
{

}

CursorTextureProvider *WQuickCursorPrivate::tp() const
{
    if (!textureProvider) {
        Q_ASSERT(cursorImage);
        textureProvider = new CursorTextureProvider;
        updateTextureConnection = QObject::connect(cursorImage, SIGNAL(imageChanged()),
                                                   q_func(), SLOT(updateTexture()));
        Q_ASSERT(updateTextureConnection);

        if (cursorSurfaceItem) {
            Q_ASSERT(cursorSurfaceItem->surface());
            textureProvider->setProxy(cursorSurfaceItem->wTextureProvider());
        } else {
            const_cast<WQuickCursorPrivate*>(this)->updateTexture();
        }
    }
    return textureProvider;
}

void WQuickCursorPrivate::cleanTextureProvider()
{
    if (textureProvider) {
        class WQuickCursorCleanupJob : public QRunnable
        {
        public:
            WQuickCursorCleanupJob(QObject *object) : m_object(object) { }
            void run() override {
                delete m_object;
            }
            QObject *m_object;
        };

        // Delay clean the textures on the next render after.
        window->scheduleRenderJob(new WQuickCursorCleanupJob(textureProvider),
                                  QQuickWindow::AfterRenderingStage);
        textureProvider = nullptr;
    }
}

void WQuickCursorPrivate::setSurface(WSurface *surface)
{
    W_Q(WQuickCursor);

    if (surface) {
        if (!cursorSurfaceItem) {
            cursorSurfaceItem = new WSurfaceItemContent(q);
            cursorSurfaceItem->setIgnoreBufferOffset(true);
            QQuickItemPrivate::get(cursorSurfaceItem)->anchors()->setFill(q);
            bool ok = QObject::connect(cursorSurfaceItem, SIGNAL(implicitWidthChanged()),
                                       q, SLOT(updateImplicitSize()));
            Q_ASSERT(ok);
            ok = QObject::connect(cursorSurfaceItem, SIGNAL(implicitHeightChanged()),
                                  q, SLOT(updateImplicitSize()));
            Q_ASSERT(ok);
            QObject::connect(cursorSurfaceItem, &WSurfaceItemContent::bufferOffsetChanged,
                             q, [this] {
                setHotSpot(hotSpot -= cursorSurfaceItem->bufferOffset());
            });

            q->setFlag(QQuickItem::ItemHasContents, false);
            q->update();
        }

        cursorSurfaceItem->setSurface(surface);
        if (textureProvider)
            textureProvider->setProxy(cursorSurfaceItem->wTextureProvider());
        Q_ASSERT(!q->flags().testFlag(QQuickItem::ItemHasContents));

        if (q->isVisible())
            enterOutput(output);
    } else {
        if (cursorSurfaceItem) {
            leaveOutput(output);
            cursorSurfaceItem->deleteLater();
            cursorSurfaceItem = nullptr;
        }
        q->setFlag(QQuickItem::ItemHasContents, true);
        q->update();
    }

    updateImplicitSize();
}

void WQuickCursorPrivate::enterOutput(WOutput *output)
{
    if (!output)
        return;
    if (!cursorSurfaceItem)
        return;
    auto surface = cursorSurfaceItem->surface();
    if (!surface)
        return;
    surface->enterOutput(output);
}

void WQuickCursorPrivate::leaveOutput(WOutput *output)
{
    if (!output)
        return;
    if (!cursorSurfaceItem)
        return;
    auto surface = cursorSurfaceItem->surface();
    if (!surface)
        return;
    surface->leaveOutput(output);
}

void WQuickCursorPrivate::updateXCursorManager()
{
    cursorImage->setCursorTheme(xcursorThemeName.toLatin1(), getCursorSize());
    cursorImage->setScale(window ? window->effectiveDevicePixelRatio() : 1.0);
}

void WQuickCursorPrivate::updateTexture()
{
    textureProvider->setImage(cursorImage->image(), window);
    updateImplicitSize();

    if (!cursorSurfaceItem) {
        setHotSpot(cursorImage->hotSpot());
        q_func()->update();
    }
}

void WQuickCursorPrivate::updateCursor()
{
    auto cursor = this->cursor->cursor();
    if (WGlobal::isClientResourceCursor(cursor)) {
        // First try use cursor shape
        auto shape = this->cursor->requestedCursorShape();
        if (shape != WGlobal::CursorShape::Invalid) {
            setSurface(nullptr);
            cursorImage->setCursor(WCursor::toQCursor(shape));
            setHotSpot(cursorImage->hotSpot());
        } else { // Second use cursor surface
            auto rs = this->cursor->requestedCursorSurface();
            cursorImage->setCursor(QCursor(Qt::BlankCursor));
            setSurface(rs.first);
            setHotSpot(rs.second);
        }
    } else {
        setSurface(nullptr);
        cursorImage->setCursor(cursor);
        setHotSpot(cursorImage->hotSpot());
    }

    Q_EMIT q_func()->validChanged();
}

void WQuickCursorPrivate::updateImplicitSize()
{
    W_Q(WQuickCursor);

    if (cursorSurfaceItem) {
        q->setImplicitSize(cursorSurfaceItem->implicitWidth(),
                           cursorSurfaceItem->implicitHeight());
    } else if (const QImage &i = cursorImage->image(); !i.isNull()) {
        const auto size = i.deviceIndependentSize();
        q->setImplicitSize(size.width(), size.height());
    } else {
        q->setImplicitSize(cursorSize.width(), cursorSize.height());
    }

    Q_EMIT q->hotSpotChanged();
}

WQuickCursor::WQuickCursor(QQuickItem *parent)
    : QQuickItem(*new WQuickCursorPrivate(this), parent)
{
    W_D(WQuickCursor);
    d->cursorImage = new WCursorImage(this);
    setFlag(QQuickItem::ItemHasContents);
    setImplicitSize(d->cursorSize.width(), d->cursorSize.height());
}

WQuickCursor::~WQuickCursor()
{
    // `d->window` will become nullptr in ~QQuickItem
    // cleanTextureProvider in ~WQuickCursorPrivate is too late
    d_func()->cleanTextureProvider();
}

WQuickCursorAttached *WQuickCursor::qmlAttachedProperties(QObject *target)
{
    if (!target->isQuickItemType())
        return nullptr;
    return new WQuickCursorAttached(qobject_cast<QQuickItem*>(target));
}

QSGTextureProvider *WQuickCursor::textureProvider() const
{
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    W_DC(WQuickCursor);
    return d->tp();
}

bool WQuickCursor::isTextureProvider() const
{
    return true;
}

bool WQuickCursor::valid() const
{
    W_DC(WQuickCursor);
    if (!d->cursor || !d->cursorImage)
        return false;

    return d->cursorSurfaceItem
           ||  !WGlobal::isInvalidCursor(d->cursorImage->cursor());
}

WCursor *WQuickCursor::cursor() const
{
    W_DC(WQuickCursor);
    return d->cursor;
}

void WQuickCursor::setCursor(WCursor *cursor)
{
    W_D(WQuickCursor);

    if (d->cursor == cursor)
        return;

    if (d->cursor) {
        Q_ASSERT(d->cursor->eventWindow() == window());
        d->cursor->setEventWindow(nullptr);
        bool ok = QObject::disconnect(d->cursor, nullptr, this, nullptr);
        Q_ASSERT(ok);
    }
    d->cursor = cursor;

    if (d->cursor) {
        bool ok = connect(d->cursor, SIGNAL(cursorChanged()), this, SLOT(updateCursor()));
        Q_ASSERT(ok);

        ok = QObject::connect(d->cursor, SIGNAL(requestedCursorShapeChanged()),
                              this, SLOT(updateCursor()));
        Q_ASSERT(ok);
        ok = QObject::connect(d->cursor, SIGNAL(requestedCursorSurfaceChanged()),
                              this, SLOT(updateCursor()));
        Q_ASSERT(ok);

        if (isComponentComplete()) {
            Q_ASSERT(!d->cursor->eventWindow()
                     || d->cursor->eventWindow() == window());
            d->cursor->setEventWindow(window());
            d->updateXCursorManager();
            d->updateCursor();
        }
    }

    Q_EMIT cursorChanged();
}

QString WQuickCursor::themeName() const
{
    W_DC(WQuickCursor);
    return d->xcursorThemeName;
}

void WQuickCursor::setThemeName(const QString &name)
{
    W_D(WQuickCursor);

    if (d->xcursorThemeName == name)
        return;
    d->xcursorThemeName = name;
    if (isComponentComplete())
        QMetaObject::invokeMethod(this, "updateXCursorManager", Qt::QueuedConnection);
}

QSize WQuickCursor::sourceSize() const
{
    W_DC(WQuickCursor);
    return d->cursorSize;
}

void WQuickCursor::setSourceSize(const QSize &size)
{
    W_D(WQuickCursor);

    if (d->cursorSize == size)
        return;
    d->cursorSize = size;
    if (isComponentComplete())
        QMetaObject::invokeMethod(this, "updateXCursorManager", Qt::QueuedConnection);
}

QPointF WQuickCursor::hotSpot() const
{
    W_DC(WQuickCursor);

    if (d->cursorSurfaceItem) {
        return QPointF(d->hotSpot) * (width() / d->cursorSurfaceItem->implicitWidth());
    } else if (d->cursorImage && !d->cursorImage->image().isNull()) {
        return QPointF(d->hotSpot) * (width() / d->cursorImage->image().width());
    }

    return {};
}

WOutput *WQuickCursor::output() const
{
    W_DC(WQuickCursor);
    return d->output;
}

void WQuickCursor::setOutput(WOutput *newOutput)
{
    W_D(WQuickCursor);
    if (d->output == newOutput)
        return;

    if (isVisible()) {
        d->enterOutput(newOutput);
        d->leaveOutput(d->output);
    }

    d->output = newOutput;
    Q_EMIT outputChanged();
}

void WQuickCursor::invalidateSceneGraph()
{
    W_D(WQuickCursor);
    delete d->textureProvider;
    d->textureProvider = nullptr;
}

void WQuickCursor::componentComplete()
{
    W_D(WQuickCursor);

    if (d->cursor) {
        Q_ASSERT(!d->cursor->eventWindow()
                 || d->cursor->eventWindow() == window());
        d->cursor->setEventWindow(window());
        d->updateXCursorManager();
        d->updateCursor();
    }

    QQuickItem::componentComplete();
}

void WQuickCursor::itemChange(ItemChange change, const ItemChangeData &data)
{
    W_D(WQuickCursor);
    if (change == ItemSceneChange) {
        if (d->cursor)
            d->cursor->setEventWindow(data.window);

        if (data.window) {
            auto rw = qobject_cast<WOutputRenderWindow*>(data.window);
            Q_ASSERT(rw);
            bool ok = QObject::connect(rw, SIGNAL(initialized()), this, SLOT(updateTexture()));
            Q_ASSERT(ok);
        }
    } else if (change == ItemDevicePixelRatioHasChanged) {
        d->updateXCursorManager();
    } else if (change == ItemVisibleHasChanged) {
        // The visible state is set by compositor(following WOutputCursor::visible property on default)
        if (data.boolValue) {
            d->enterOutput(d->output);
        } else {
            d->leaveOutput(d->output);
        }
    }

    QQuickItem::itemChange(change, data);
}

void WQuickCursor::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    W_DC(WQuickCursor);
    if (d->hotSpot.isNull() && newGeometry.size() != oldGeometry.size())
        Q_EMIT hotSpotChanged();
    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

QSGNode *WQuickCursor::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    W_DC(WQuickCursor);

    Q_ASSERT(!d->cursorSurfaceItem);
    auto tp = d->tp();
    Q_ASSERT(tp);
    // Ignore the tp->proxy, Don't use tp->qwBuffer()
    if (!tp->buffer) {
        delete node;
        return nullptr;
    }

    auto texture = &tp->m_texture;
    auto imageNode = static_cast<QSGImageNode*>(node);
    if (!imageNode)
        imageNode = window()->createImageNode();

    imageNode->setTexture(texture);
    imageNode->setOwnsTexture(false);
    imageNode->setSourceRect(QRectF(QPointF(0, 0), texture->textureSize()));
    imageNode->setRect(QRectF(QPointF(0, 0), QSizeF(width(), height())));
    imageNode->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    imageNode->setMipmapFiltering(QSGTexture::None);
    imageNode->setAnisotropyLevel(antialiasing() ? QSGTexture::Anisotropy4x : QSGTexture::AnisotropyNone);

    return imageNode;
}

void WQuickCursor::releaseResources()
{
    W_D(WQuickCursor);

    d->invalidate();

    // Force to update the contents, avoid to render the invalid textures
    QQuickItemPrivate::get(this)->dirty(QQuickItemPrivate::Content);
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickcursor.cpp"
