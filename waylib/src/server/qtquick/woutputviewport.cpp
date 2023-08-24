// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputviewport.h"
#include "woutputviewport_p.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wquickseat_p.h"
#include "wseat.h"
#include "wtools.h"

#include <private/qsgplaintexture_p.h>

extern "C" {
#include <wlr/render/wlr_texture.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
}

WAYLIB_SERVER_BEGIN_NAMESPACE

QSGTexture *CursorTextureFactory::createTexture(QQuickWindow *window) const
{
    std::unique_ptr<QSGPlainTexture> texture(new QSGPlainTexture());
    if (!WTexture::makeTexture(this->texture, texture.get(), window))
        return nullptr;

    texture->setOwnsTexture(false);

    if (!texture->image().isNull()) {
        // Can't use QSGPlainTexture for QQuickImage on software renderer
        auto flags = texture->hasAlphaChannel() ? QQuickWindow::TextureHasAlphaChannel : QQuickWindow::CreateTextureOptions{0};
        return window->createTextureFromImage(texture->image(), flags);
    }

    return texture.release();
}

QImage CursorTextureFactory::image() const
{
    if (!wlr_texture_is_pixman(texture->handle())) {
        return {};
    }

    auto image = wlr_pixman_texture_get_image(texture->handle());
    return WTools::fromPixmanImage(image);
}

QuickOutputCursor::QuickOutputCursor(QObject *parent)
    : QObject(parent)
{

}

QuickOutputCursor::~QuickOutputCursor()
{
    if (delegateItem)
        delegateItem->deleteLater();
}

QString QuickOutputCursor::imageProviderId()
{
    return QLatin1StringView("QuickOutputCursor");
}

bool QuickOutputCursor::visible() const
{
    return m_visible;
}

void QuickOutputCursor::setVisible(bool newVisible)
{
    if (m_visible == newVisible)
        return;
    m_visible = newVisible;
    Q_EMIT visibleChanged();
}

bool QuickOutputCursor::isHardwareCursor() const
{
    return m_isHardwareCursor;
}

void QuickOutputCursor::setIsHardwareCursor(bool newIsHardwareCursor)
{
    if (m_isHardwareCursor == newIsHardwareCursor)
        return;
    m_isHardwareCursor = newIsHardwareCursor;
    Q_EMIT isHardwareCursorChanged();
}

QPointF QuickOutputCursor::hotspot() const
{
    return m_hotspot;
}

void QuickOutputCursor::setHotspot(QPointF newHotspot)
{
    if (m_hotspot == newHotspot)
        return;
    m_hotspot = newHotspot;
    Q_EMIT hotspotChanged();
}

void QuickOutputCursor::setTexture(wlr_texture *texture)
{
    // Though "lastTexture == texture", but maybe the texture native resource
    // is changed(like as OpenGL texture id changed). Need to check TextureAttrib
    // whether to change
    TextureAttrib texAttr;
    if (!texture) {
        texAttr.type = TextureAttrib::EMPTY;
    } else if (wlr_texture_is_gles2(texture)) {
        wlr_gles2_texture_attribs attr;
        wlr_gles2_texture_get_attribs(texture, &attr);
        texAttr.type = TextureAttrib::GLES;
        texAttr.tex = attr.tex;
    } else if (wlr_texture_is_pixman(texture)) {
        texAttr.type = TextureAttrib::PIXMAN;
        texAttr.pimage = wlr_pixman_texture_get_image(texture);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_texture_is_vk(texture)) {
        wlr_vk_image_attribs attr;
        wlr_vk_texture_get_image_attribs(texture, &attr);
        texAttr.type = TextureAttrib::VULKAN;
        texAttr.vimage = attr.image;
    }
#endif

    Q_ASSERT_X(texAttr.type != TextureAttrib::INVALID, "QuickOutputCursor::setTexture", "Unknow texture type");

    if (lastTexture == texture && lastTextureAttrib == texAttr)
        return;

    lastTexture = texture;
    lastTextureAttrib = texAttr;

    QUrl url;

    if (texture) {
        url.setScheme("image");
        url.setHost(imageProviderId());
        url.setPath(QString("/%1/%2").arg(quintptr(QWTexture::from(texture)), 0, 16));
    }

    setImageSource(url);
}

QUrl QuickOutputCursor::imageSource() const
{
    return m_imageSource;
}

void QuickOutputCursor::setImageSource(const QUrl &newImageSource)
{
    if (m_imageSource == newImageSource)
        return;
    m_imageSource = newImageSource;
    Q_EMIT imageSourceChanged();
}

QSizeF QuickOutputCursor::size() const
{
    return m_size;
}

void QuickOutputCursor::setSize(const QSizeF &newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;
    Q_EMIT sizeChanged();
}

QRectF QuickOutputCursor::sourceRect() const
{
    return m_sourceRect;
}

void QuickOutputCursor::setSourceRect(const QRectF &newSourceRect)
{
    if (m_sourceRect == newSourceRect)
        return;
    m_sourceRect = newSourceRect;
    emit sourceRectChanged();
}

void QuickOutputCursor::setPosition(const QPointF &pos)
{
    delegateItem->setPosition(delegateItem->parentItem()->mapFromGlobal(pos));
}

void QuickOutputCursor::setDelegateItem(QQuickItem *item)
{
    delegateItem = item;
}

QQuickTextureFactory *CursorProvider::requestTexture(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize);

    auto tmp = id.split('/');
    if (tmp.isEmpty())
        return nullptr;
    bool ok = false;
    QWTexture *texture = QWTexture::from(reinterpret_cast<wlr_texture*>(tmp.first().toLongLong(&ok, 16)));
    if (!ok)
        return nullptr;

    CursorTextureFactory *factory = new CursorTextureFactory(texture);

    if (size)
        *size = factory->textureSize();

    return factory;
}

void WOutputViewportPrivate::initForOutput()
{
    W_Q(WOutputViewport);

    outputWindow()->attach(q);

    auto qwoutput = output->handle();
    if (!qwoutput->handle()->current_mode
        && !qwoutput->handle()->pending.mode) {
        auto mode = qwoutput->preferredMode();
        if (mode)
            qwoutput->setMode(mode);
    }

    qwoutput->enable(q->isVisible());
    // Must commit after init render for the output, the init render
    // behavior is in WOutputRenderWindow::attach
    if (qwoutput->handle()->renderer)
        qwoutput->commit();

    QObject::connect(output, &WOutput::modeChanged, q, [this] {
        updateImplicitSize();
    });

    updateImplicitSize();
    clearCursors();
}

qreal WOutputViewportPrivate::getImplicitWidth() const
{
    return output->size().width() / devicePixelRatio;
}

qreal WOutputViewportPrivate::getImplicitHeight() const
{
    return output->size().height() / devicePixelRatio;
}

void WOutputViewportPrivate::updateImplicitSize()
{
    implicitWidthChanged();
    implicitHeightChanged();

    W_Q(WOutputViewport);
    q->resetWidth();
    q->resetHeight();
}

void WOutputViewportPrivate::setVisible(bool visible)
{
    QQuickItemPrivate::setVisible(visible);
    if (output)
        output->handle()->enable(visible);
}

void WOutputViewportPrivate::clearCursors()
{
    for (auto i : cursors)
        i->deleteLater();
    cursors.clear();
}

void WOutputViewportPrivate::updateCursors()
{
    if (!output || !cursorDelegate)
        return;

    W_Q(WOutputViewport);

    int index = 0;
    struct wlr_output_cursor *cursor;
    wl_list_for_each(cursor, &output->handle()->handle()->cursors, link) {
        QuickOutputCursor *quickCursor = nullptr;
        if (index >= cursors.count()) {
            quickCursor = new QuickOutputCursor(q);
            auto obj = cursorDelegate->createWithInitialProperties({{"cursor", QVariant::fromValue(quickCursor)}}, qmlContext(q));
            auto item = qobject_cast<QQuickItem*>(obj);

            if (!item)
                qFatal("Must using Item for the Cursor delegate");

            QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
            item->setZ(qreal(WOutputLayout::Layer::Cursor));
            Q_ASSERT(window);
            item->setParentItem(window->contentItem());

            quickCursor->setDelegateItem(item);
            cursors.append(quickCursor);
        } else {
            quickCursor = cursors.at(index);
        }

        quickCursor->setVisible(cursor->visible);
        quickCursor->setIsHardwareCursor(output->handle()->handle()->hardware_cursor == cursor);
        const QPointF position = QPointF(cursor->x, cursor->y) / cursor->output->scale;
        quickCursor->setPosition(position);
        quickCursor->setHotspot((QPointF(cursor->hotspot_x, cursor->hotspot_y) / cursor->output->scale).toPoint());
        quickCursor->setSize(QSizeF(cursor->width, cursor->height) / cursor->output->scale);
        quickCursor->setSourceRect(QRectF(cursor->src_box.x, cursor->src_box.y, cursor->src_box.width, cursor->src_box.height));
        quickCursor->setTexture(cursor->texture);

        ++index;
    }

    // clean needless cursors
    for (int i = index + 1; i < cursors.count(); ++i) {
        cursors.at(i)->deleteLater();
        cursors.removeAt(i);
    }
}

WOutputViewport::WOutputViewport(QQuickItem *parent)
    : QQuickItem(*new WOutputViewportPrivate(), parent)
{

}

WOutputViewport::~WOutputViewport()
{
    W_D(WOutputViewport);
    if (d->componentComplete && d->output && d->window)
        d->outputWindow()->detach(this);
}

#define DATA_OF_WOUPTUT "_WOutputViewport"

WOutputViewport *WOutputViewport::get(WOutput *output)
{
    return qvariant_cast<WOutputViewport*>(output->property(DATA_OF_WOUPTUT));
}

WOutput *WOutputViewport::output() const
{
    W_D(const WOutputViewport);
    return d->output;
}

void WOutputViewport::setOutput(WOutput *newOutput)
{
    W_D(WOutputViewport);

    Q_ASSERT(!d->output || !newOutput);
    if (newOutput)
        newOutput->setProperty(DATA_OF_WOUPTUT, QVariant::fromValue(this));

    if (d->componentComplete) {
        if (newOutput) {
            d->output = newOutput;
            d->initForOutput();
            if (d->seat)
                d->seat->addOutput(d->output);
        } else {
            if (d->seat)
                d->seat->removeOutput(d->output);
        }
    }

    d->output = newOutput;
}

WQuickSeat *WOutputViewport::seat() const
{
    W_D(const WOutputViewport);
    return d->seat;
}

// Bind this seat to output
void WOutputViewport::setSeat(WQuickSeat *newSeat)
{
    W_D(WOutputViewport);

    if (d->seat == newSeat)
        return;
    d->seat = newSeat;

    if (d->componentComplete && d->output)
        d->seat->addOutput(d->output);

    Q_EMIT seatChanged();
}

qreal WOutputViewport::devicePixelRatio() const
{
    W_DC(WOutputViewport);
    return d->devicePixelRatio;
}

void WOutputViewport::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputViewport);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

QQmlComponent *WOutputViewport::cursorDelegate() const
{
    W_DC(WOutputViewport);
    return d->cursorDelegate;
}

void WOutputViewport::setCursorDelegate(QQmlComponent *delegate)
{
    W_D(WOutputViewport);
    if (d->cursorDelegate == delegate)
        return;

    d->cursorDelegate = delegate;
    d->clearCursors();

    Q_EMIT cursorDelegateChanged();
}

void WOutputViewport::classBegin()
{
    W_D(WOutputViewport);

    QQuickItem::classBegin();
    auto engine = qmlEngine(this);

    if (!engine->imageProvider(QuickOutputCursor::imageProviderId()))
        engine->addImageProvider(QuickOutputCursor::imageProviderId(), new CursorProvider());
}

void WOutputViewport::componentComplete()
{
    W_D(WOutputViewport);

    if (d->output) {
        d->initForOutput();
        if (d->seat)
            d->seat->addOutput(d->output);
    }

    QQuickItem::componentComplete();
}

void WOutputViewport::releaseResources()
{
    W_D(WOutputViewport);
    if (d->seat)
        d->seat->removeOutput(d_func()->output);
    QQuickItem::releaseResources();
}

void WOutputViewport::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemChange::ItemSceneChange) {
        W_D(WOutputViewport);
        d->clearCursors();
        if (d->updateCursorsConnection)
            disconnect(d->updateCursorsConnection);
        if (data.window) {
            d->updateCursorsConnection = connect(data.window, SIGNAL(beforeSynchronizing()), this, SLOT(updateCursors()));
            d->updateCursors();
        }
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputviewport.cpp"
