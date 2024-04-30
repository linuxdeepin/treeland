// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputitem.h"
#include "woutputitem_p.h"
#include "woutputrenderwindow.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wquickseat_p.h"
#include "wquickoutputlayout.h"
#include "wseat.h"
#include "wquickseat_p.h"
#include "wtools.h"
#include "wtexture.h"
#include "wquickcursor.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>

#include <private/qquickitem_p.h>
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

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputItemAttached::WOutputItemAttached(QObject *parent)
    : QObject(parent)
{

}

WOutputItem *WOutputItemAttached::item() const
{
    return m_positioner;
}

void WOutputItemAttached::setItem(WOutputItem *positioner)
{
    if (m_positioner == positioner)
        return;
    m_positioner = positioner;
    Q_EMIT itemChanged();
}

class CursorTextureFactory : public QQuickTextureFactory
{
public:
    CursorTextureFactory(QWTexture *texture)
        : QQuickTextureFactory()
        , texture(texture)
    {

    }

    QSGTexture *createTexture(QQuickWindow *window) const override;
    QSize textureSize() const override {
        return QSize(texture->handle()->width, texture->handle()->height);
    }
    int textureByteCount() const override {
        const QSize size = textureSize();
        // ###(zccrs): Don't know byte count of wlr_texture
        return size.width() * size.height() * 4;
    }
    QImage image() const override;

private:
    QWTexture *texture;
};

class CursorProvider : public QQuickImageProvider
{
public:
    CursorProvider()
        : QQuickImageProvider(QQmlImageProviderBase::Texture)
    {

    }

    QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize) override;
};

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

QuickOutputCursor::QuickOutputCursor(wlr_output_cursor *handle, QObject *parent)
    : QObject(parent)
    , m_handle(handle)
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

void QuickOutputCursor::setHandle(wlr_output_cursor *handle)
{
    m_handle = handle;
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

bool QuickOutputCursor::setPosition(const QPointF &pos)
{
    const auto newPosition = delegateItem->parentItem()->mapFromGlobal(pos);
    if (newPosition == delegateItem->position())
        return false;
    // TODO: Don't change position if the item isn't visible, change position
    // will lead to the window repaint.
    delegateItem->setPosition(newPosition);
    return true;
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

#define DATA_OF_WOUPTUT "_WOutputItem"
class WOutputItemPrivate : public WObjectPrivate
{
public:
    WOutputItemPrivate(WOutputItem *qq)
        : WObjectPrivate(qq)
    {

    }
    ~WOutputItemPrivate() {
        clearCursors();

        if (layout)
            layout->remove(q_func());
        if (output)
            output->setProperty(DATA_OF_WOUPTUT, QVariant());
    }

    void initForOutput();

    void updateImplicitSize() {
        W_Q(WOutputItem);

        q->privateImplicitWidthChanged();
        q->privateImplicitHeightChanged();

        q->resetWidth();
        q->resetHeight();
    }

    void clearCursors();
    void updateCursors();
    QuickOutputCursor *getCursorBy(wlr_output_cursor *handle) const;

    W_DECLARE_PUBLIC(WOutputItem)
    QPointer<WOutput> output;
    QPointer<WQuickOutputLayout> layout;
    qreal devicePixelRatio = 1.0;

    WQuickSeat *seat = nullptr;
    QQmlComponent *cursorDelegate = nullptr;
    QList<QuickOutputCursor*> cursors;
    QuickOutputCursor *lastActiveCursor = nullptr;
    QMetaObject::Connection updateCursorsConnection;
};

void WOutputItemPrivate::initForOutput()
{
    W_Q(WOutputItem);
    Q_ASSERT(output);

    if (layout)
        layout->add(q);

    QObject::connect(output, &WOutput::transformedSizeChanged, q, [this] {
        updateImplicitSize();
    });

    updateImplicitSize();
    clearCursors();
}

void WOutputItemPrivate::clearCursors()
{
    for (auto i : cursors)
        i->deleteLater();
    cursors.clear();
}

void WOutputItemPrivate::updateCursors()
{
    if (!output || !cursorDelegate)
        return;

    W_Q(WOutputItem);

    QList<QuickOutputCursor*> tmpCursors;
    tmpCursors.reserve(cursors.size());
    bool cursorsChanged = false;

    struct wlr_output_cursor *cursor;
    wl_list_for_each(cursor, &output->handle()->handle()->cursors, link) {
        QuickOutputCursor *quickCursor = getCursorBy(cursor);
        if (!quickCursor) {
            quickCursor = new QuickOutputCursor(cursor, q);
            auto obj = cursorDelegate->createWithInitialProperties({{"cursor", QVariant::fromValue(quickCursor)}}, qmlContext(q));
            auto item = qobject_cast<QQuickItem*>(obj);

            if (!item)
                qFatal("Cursor delegate must is Item");

            QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
            item->setZ(qreal(WOutputLayout::Layer::Cursor));
            Q_ASSERT(q->window());
            item->setParentItem(q->window()->contentItem());

            quickCursor->setDelegateItem(item);
            cursorsChanged = true;
        }

        tmpCursors.append(quickCursor);

        quickCursor->setVisible(cursor->visible);
        quickCursor->setIsHardwareCursor(output->handle()->handle()->hardware_cursor == cursor);
        const QPointF position = QPointF(cursor->x, cursor->y) / cursor->output->scale + output->position();
        bool positionChanged = quickCursor->setPosition(position);
        quickCursor->setHotspot((QPointF(cursor->hotspot_x, cursor->hotspot_y) / cursor->output->scale).toPoint());
        quickCursor->setSize(QSizeF(cursor->width, cursor->height) / cursor->output->scale);
        quickCursor->setSourceRect(QRectF(cursor->src_box.x, cursor->src_box.y, cursor->src_box.width, cursor->src_box.height));
        quickCursor->setTexture(cursor->texture);

        if (cursor->visible && positionChanged && lastActiveCursor != quickCursor) {
            lastActiveCursor = quickCursor;
            Q_EMIT q->lastActiveCursorItemChanged();
        }
    }

    std::swap(tmpCursors, cursors);
    // clean needless cursors
    for (auto cursor : tmpCursors) {
        if (cursors.contains(cursor))
            continue;
        if (cursor == lastActiveCursor) {
            lastActiveCursor = nullptr;
            Q_EMIT q->lastActiveCursorItemChanged();
        }
        cursor->deleteLater();
        cursorsChanged = true;
    }

    if (lastActiveCursor && !lastActiveCursor->visible()) {
        lastActiveCursor = nullptr;
        Q_EMIT q->lastActiveCursorItemChanged();
    }

    if (cursorsChanged)
        Q_EMIT q->cursorItemsChanged();
}

QuickOutputCursor *WOutputItemPrivate::getCursorBy(wlr_output_cursor *handle) const
{
    for (auto cursor : cursors)
        if (cursor->m_handle == handle)
            return cursor;
    return nullptr;
}

WOutputItem::WOutputItem(QQuickItem *parent)
    : WQuickObserver(parent)
    , WObject(*new WOutputItemPrivate(this))
{

}

WOutputItem::~WOutputItem()
{

}

WOutputItemAttached *WOutputItem::qmlAttachedProperties(QObject *target)
{
    auto output = qobject_cast<WOutput*>(target);
    if (!output)
        return nullptr;
    auto attached = new WOutputItemAttached(output);
    attached->setItem(qvariant_cast<WOutputItem*>(output->property(DATA_OF_WOUPTUT)));

    return attached;
}

WOutput *WOutputItem::output() const
{
    W_D(const WOutputItem);
    return d->output.get();
}

inline static WOutputItemAttached *getAttached(WOutput *output)
{
    return output->findChild<WOutputItemAttached*>(QString(), Qt::FindDirectChildrenOnly);
}

void WOutputItem::setOutput(WOutput *newOutput)
{
    W_D(WOutputItem);

    Q_ASSERT(!d->output || !newOutput);
    auto oldOutput = d->output;
    d->output = newOutput;

    if (newOutput) {
        if (auto attached = getAttached(newOutput)) {
            attached->setItem(this);
        } else {
            newOutput->setProperty(DATA_OF_WOUPTUT, QVariant::fromValue(this));
        }
    }

    if (isComponentComplete()) {
        if (newOutput) {
            d->initForOutput();
            if (d->seat)
                d->seat->addOutput(d->output);
        } else {
            if (d->seat)
                d->seat->removeOutput(oldOutput);
        }
    }
}

WQuickOutputLayout *WOutputItem::layout() const
{
    Q_D(const WOutputItem);
    return d->layout.get();
}

void WOutputItem::setLayout(WQuickOutputLayout *layout)
{
    Q_D(WOutputItem);

    if (d->layout == layout)
        return;

    if (d->layout)
        d->layout->remove(this);

    d->layout = layout;
    if (isComponentComplete() && d->layout && d->output)
        d->layout->add(this);

    Q_EMIT layoutChanged();
}

qreal WOutputItem::devicePixelRatio() const
{
    W_DC(WOutputItem);
    return d->devicePixelRatio;
}

void WOutputItem::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputItem);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

WQuickSeat *WOutputItem::seat() const
{
    W_D(const WOutputItem);
    return d->seat;
}

// Bind this seat to output
void WOutputItem::setSeat(WQuickSeat *newSeat)
{
    W_D(WOutputItem);

    if (d->seat == newSeat)
        return;
    d->seat = newSeat;

    if (isComponentComplete() && d->output)
        d->seat->addOutput(d->output);

    Q_EMIT seatChanged();
}

QQmlComponent *WOutputItem::cursorDelegate() const
{
    W_DC(WOutputItem);
    return d->cursorDelegate;
}

void WOutputItem::setCursorDelegate(QQmlComponent *delegate)
{
    W_D(WOutputItem);
    if (d->cursorDelegate == delegate)
        return;

    d->cursorDelegate = delegate;
    d->clearCursors();

    Q_EMIT cursorDelegateChanged();
}

QQuickItem *WOutputItem::lastActiveCursorItem() const
{
    W_DC(WOutputItem);
    return d->lastActiveCursor ? d->lastActiveCursor->delegateItem : nullptr;
}

QList<QQuickItem *> WOutputItem::cursorItems() const
{
    W_DC(WOutputItem);

    QList<QQuickItem *> items;
    items.reserve(d->cursors.size());

    for (auto cursor : d->cursors)
        items.append(cursor->delegateItem);

    return items;
}

void WOutputItem::classBegin()
{
    W_D(WOutputItem);

    QQuickItem::classBegin();
    auto engine = qmlEngine(this);

    if (!engine->imageProvider(QuickOutputCursor::imageProviderId()))
        engine->addImageProvider(QuickOutputCursor::imageProviderId(), new CursorProvider());
}

void WOutputItem::componentComplete()
{
    W_D(WOutputItem);

    if (d->output) {
        d->initForOutput();
        if (d->seat)
            d->seat->addOutput(d->output);
    }

    WQuickObserver::componentComplete();
}

void WOutputItem::releaseResources()
{
    W_D(WOutputItem);
    if (d->seat)
        d->seat->removeOutput(d_func()->output);
    WQuickObserver::releaseResources();
}

void WOutputItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemChange::ItemSceneChange) {
        W_D(WOutputItem);
        d->clearCursors();
        if (d->updateCursorsConnection)
            disconnect(d->updateCursorsConnection);
        if (data.window) {
            d->updateCursorsConnection = connect(data.window, SIGNAL(beforeSynchronizing()), this, SLOT(updateCursors()));
            d->updateCursors();
        }
    }
}

qreal WOutputItem::getImplicitWidth() const
{
    W_DC(WOutputItem);
    return d->output->transformedSize().width() / d->devicePixelRatio;
}

qreal WOutputItem::getImplicitHeight() const
{
    W_DC(WOutputItem);
    return d->output->transformedSize().height() / d->devicePixelRatio;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputitem.cpp"
