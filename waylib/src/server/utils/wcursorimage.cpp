// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wcursorimage.h"
#include "wcursor.h"

#include <qwxcursormanager.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QTimer>
#include <private/qobject_p.h>

#include <memory>

Q_LOGGING_CATEGORY(qLcCursorImage, "waylib.server.cursor.image", QtWarningMsg)

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

static inline wlr_xcursor *getXCursorWithFallback(qw_xcursor_manager *xcursor_manager, const char *name, float scale)
{
    if (!xcursor_manager)
        return nullptr;

    if (wlr_xcursor *cursor = xcursor_manager->get_xcursor(name, scale)) {
        return cursor;
    }

    static const std::vector<std::vector<const char*>> typeLists = {
        {"ibeam", "text", "xterm"},
        {"openhand", "grab"},
        {"crosshair", "cross", "all-scroll"},
        {"closedhand", "dnd-move", "move", "dnd-none", "grabbing"},
        {"dnd-copy", "copy"},
        {"dnd-link", "link"},
        {"row-resize", "size_ver", "ns-resize", "split_v", "n-resize", "s-resize"},
        {"col-resize", "size_hor", "ew-resize", "split_h", "e-resize", "w-resize"},
        {"nwse-resize", "nw-resize", "se-resize", "size_fdiag"},
        {"progress", "wait", "watch"},
        {"hand1", "hand2", "pointer", "pointing_hand"},
    };

    for (const auto &typeList : typeLists) {
        bool hasType = std::find_if(typeList.begin(), typeList.end(), [name](const char *type) {
                           return std::strcmp(type, name) == 0;
                       }) != typeList.end();

        if (!hasType)
            continue;

        for (const auto &type : typeList) {
            if (std::strcmp(type, name) == 0)
                continue;

            if (auto cursor = xcursor_manager->get_xcursor(type, scale))
                return cursor;
        }

        return nullptr;
    }

    return nullptr;
}

static inline const char *qcursorShapeToType(std::underlying_type_t<WGlobal::CursorShape> shape) {
    switch (shape) {
    case Qt::ArrowCursor:
        return "left_ptr";
    case Qt::UpArrowCursor:
        return "up_arrow";
    case Qt::CrossCursor:
        return "cross";
    case Qt::WaitCursor:
        return "wait";
    case Qt::IBeamCursor:
        return "ibeam";
    case Qt::SizeAllCursor:
        return "size_all";
    case Qt::BlankCursor:
        return "blank";
    case Qt::PointingHandCursor:
        return "pointing_hand";
    case Qt::SizeBDiagCursor:
        return "size_bdiag";
    case Qt::SizeFDiagCursor:
        return "size_fdiag";
    case Qt::SizeVerCursor:
        return "size_ver";
    case Qt::SplitVCursor:
        return "split_v";
    case Qt::SizeHorCursor:
        return "size_hor";
    case Qt::SplitHCursor:
        return "split_h";
    case Qt::WhatsThisCursor:
        return "whats_this";
    case Qt::ForbiddenCursor:
        return "forbidden";
    case Qt::BusyCursor:
        return "left_ptr_watch";
    case Qt::OpenHandCursor:
        return "openhand";
    case Qt::ClosedHandCursor:
        return "closedhand";
    case Qt::DragCopyCursor:
        return "dnd-copy";
    case Qt::DragMoveCursor:
        return "dnd-move";
    case Qt::DragLinkCursor:
        return "dnd-link";
    case static_cast<int>(WGlobal::CursorShape::Default):
        return "default";
    case static_cast<int>(WGlobal::CursorShape::BottomLeftCorner):
        return "bottom_left_corner";
    case static_cast<int>(WGlobal::CursorShape::BottomRightCorner):
        return "bottom_right_corner";
    case static_cast<int>(WGlobal::CursorShape::TopLeftCorner):
        return "top_left_corner";
    case static_cast<int>(WGlobal::CursorShape::TopRightCorner):
        return "top_right_corner";
    case static_cast<int>(WGlobal::CursorShape::BottomSide):
        return "bottom_side";
    case static_cast<int>(WGlobal::CursorShape::LeftSide):
        return "left_side";
    case static_cast<int>(WGlobal::CursorShape::RightSide):
        return "right_side";
    case static_cast<int>(WGlobal::CursorShape::TopSide):
        return "top_side";
    case static_cast<int>(WGlobal::CursorShape::Grabbing):
        return "grabbing";
    case static_cast<int>(WGlobal::CursorShape::Xterm):
        return "xterm";
    case static_cast<int>(WGlobal::CursorShape::Hand1):
        return "hand1";
    case static_cast<int>(WGlobal::CursorShape::Watch):
        return "watch";
    case static_cast<int>(WGlobal::CursorShape::SWResize):
        return "sw-resize";
    case static_cast<int>(WGlobal::CursorShape::SEResize):
        return "se-resize";
    case static_cast<int>(WGlobal::CursorShape::SResize):
        return "s-resize";
    case static_cast<int>(WGlobal::CursorShape::WResize):
        return "w-resize";
    case static_cast<int>(WGlobal::CursorShape::EResize):
        return "e-resize";
    case static_cast<int>(WGlobal::CursorShape::EWResize):
        return "ew-resize";
    case static_cast<int>(WGlobal::CursorShape::NWResize):
        return "nw-resize";
    case static_cast<int>(WGlobal::CursorShape::NWSEResize):
        return "nwse-resize";
    case static_cast<int>(WGlobal::CursorShape::NEResize):
        return "ne-resize";
    case static_cast<int>(WGlobal::CursorShape::NESWResize):
        return "nesw-resize";
    case static_cast<int>(WGlobal::CursorShape::NSResize):
        return "ns-resize";
    case static_cast<int>(WGlobal::CursorShape::NResize):
        return "n-resize";
    case static_cast<int>(WGlobal::CursorShape::AllScroll):
        return "all-scroll";
    case static_cast<int>(WGlobal::CursorShape::Text):
        return "text";
    case static_cast<int>(WGlobal::CursorShape::Pointer):
        return "pointer";
    case static_cast<int>(WGlobal::CursorShape::Wait):
        return "wait";
    case static_cast<int>(WGlobal::CursorShape::ContextMenu):
        return "context-menu";
    case static_cast<int>(WGlobal::CursorShape::Help):
        return "help";
    case static_cast<int>(WGlobal::CursorShape::Progress):
        return "progress";
    case static_cast<int>(WGlobal::CursorShape::Cell):
        return "cell";
    case static_cast<int>(WGlobal::CursorShape::Crosshair):
        return "crosshair";
    case static_cast<int>(WGlobal::CursorShape::VerticalText):
        return "vertical-text";
    case static_cast<int>(WGlobal::CursorShape::Alias):
        return "alias";
    case static_cast<int>(WGlobal::CursorShape::Copy):
        return "copy";
    case static_cast<int>(WGlobal::CursorShape::Move):
        return "move";
    case static_cast<int>(WGlobal::CursorShape::NoDrop):
        return "no-drop";
    case static_cast<int>(WGlobal::CursorShape::NotAllowed):
        return "not-allowed";
    case static_cast<int>(WGlobal::CursorShape::Grab):
        return "grab";
    case static_cast<int>(WGlobal::CursorShape::ColResize):
        return "col-resize";
    case static_cast<int>(WGlobal::CursorShape::RowResize):
        return "row-resize";
    case static_cast<int>(WGlobal::CursorShape::ZoomIn):
        return "zoom-in";
    case static_cast<int>(WGlobal::CursorShape::ZoomOut):
        return "zoom-out";
    default:
        break;
    }

    return nullptr;
}

class Q_DECL_HIDDEN WCursorImagePrivate : public QObjectPrivate {
public:
    WCursorImagePrivate() {
        Q_ASSERT(!cursorImages.contains(this));
        cursorImages.append(this);
    }
    ~WCursorImagePrivate() {
        bool ok = cursorImages.removeOne(this);
        Q_ASSERT(ok);
    }

    void setImage(const QImage &image, const QPoint &hotspot);
    void updateCursorImage();
    void playXCursor();

    W_DECLARE_PUBLIC(WCursorImage)

    QImage image;
    QPoint hotSpot;

    QCursor cursor;
    std::shared_ptr<qw_xcursor_manager> manager;
    float scale = 1.0;

    wlr_xcursor *xcursor = nullptr;
    int currentXCursorImageIndex = 0;
    QTimer *xcursorPlayTimer = nullptr;

    static thread_local QList<WCursorImagePrivate*> cursorImages;
};
thread_local QList<WCursorImagePrivate*> WCursorImagePrivate::cursorImages;

void WCursorImagePrivate::setImage(const QImage &image, const QPoint &hotspot) {
    this->image = image;
    this->image.setDevicePixelRatio(scale);
    this->hotSpot = hotspot;
    Q_EMIT q_func()->imageChanged();
}

void WCursorImagePrivate::updateCursorImage()
{
    xcursor = nullptr;
    currentXCursorImageIndex = 0;

    std::unique_ptr<QTimer, QScopedPointerObjectDeleteLater<QTimer>> tempTimer(xcursorPlayTimer);
    xcursorPlayTimer = nullptr;
    if (tempTimer)
        tempTimer->stop();

    if (cursor.shape() == Qt::BitmapCursor) {
        setImage(cursor.pixmap().toImage(), cursor.hotSpot());
        return;
    }

    if (!manager || cursor.shape() == Qt::BlankCursor) {
        setImage(QImage(), {});
        return;
    }

    auto cursorName = qcursorShapeToType(cursor.shape());
    if (cursorName) {
        xcursor = getXCursorWithFallback(manager.get(), cursorName, scale);
        if (!xcursor)
            qCWarning(qLcCursorImage) << "Get empty cursor image for " << cursorName;
    } else {
        qCWarning(qLcCursorImage) << "Unknown cursor shape type!";
    }

    if (!xcursor || xcursor->image_count == 0) {
        setImage(QImage(), {});
        return;
    }

    if (xcursor->image_count == 1) {
        auto ximage = xcursor->images[0];
        setImage(QImage(static_cast<const uchar*>(ximage->buffer),
                        ximage->width, ximage->height,
                        QImage::Format_ARGB32_Premultiplied),
                 QPoint(ximage->hotspot_x, ximage->hotspot_y));
        return;
    }

    xcursorPlayTimer = tempTimer.release();
    if (!xcursorPlayTimer) {
        xcursorPlayTimer = new QTimer(q_func());
        xcursorPlayTimer->setSingleShot(true);
        bool ok = QObject::connect(xcursorPlayTimer, SIGNAL(timeout()),
                                   q_func(), SLOT(playXCursor()));
        Q_ASSERT(ok);
    }

    playXCursor();
}

void WCursorImagePrivate::playXCursor()
{
    Q_ASSERT(xcursor);
    Q_ASSERT(currentXCursorImageIndex < xcursor->image_count);
    Q_ASSERT(xcursorPlayTimer);
    Q_ASSERT(!xcursorPlayTimer->isActive());

    auto ximage = xcursor->images[currentXCursorImageIndex];
    setImage(QImage(static_cast<const uchar*>(ximage->buffer),
                    ximage->width, ximage->height,
                    QImage::Format_ARGB32_Premultiplied),
             QPoint(ximage->hotspot_x, ximage->hotspot_y));

    currentXCursorImageIndex = (currentXCursorImageIndex + 1) % xcursor->image_count;
    xcursorPlayTimer->start(ximage->delay);
}

WCursorImage::WCursorImage(QObject *parent)
    : QObject(*new WCursorImagePrivate(), parent)
{

}

QImage WCursorImage::image() const
{
    Q_D(const WCursorImage);
    return d->image;
}

QPoint WCursorImage::hotSpot() const
{
    Q_D(const WCursorImage);
    return d->hotSpot;
}

QCursor WCursorImage::cursor() const
{
    Q_D(const WCursorImage);
    return d->cursor;
}

void WCursorImage::setCursor(const QCursor &newCursor)
{
    Q_D(WCursorImage);
    if (d->cursor == newCursor)
        return;
    d->cursor = newCursor;
    d->updateCursorImage();

    Q_EMIT cursorChanged();
}

float WCursorImage::scale() const
{
    Q_D(const WCursorImage);
    return d->scale;
}

void WCursorImage::setScale(float newScale)
{
    Q_D(WCursorImage);
    if (qFuzzyCompare(d->scale, newScale))
        return;
    d->scale = newScale;
    if (d->manager)
        d->manager->load(d->scale);

    Q_EMIT scaleChanged();
}

static inline bool matchCursorTheme(const qw_xcursor_manager *manager,
                                    const QByteArray &name, uint32_t size) {
    return manager && manager->handle()->name == name && manager->handle()->size == size;
}

void WCursorImage::setCursorTheme(const QByteArray &name, uint32_t size)
{
    Q_D(WCursorImage);

    if (matchCursorTheme(d->manager.get(), name, size)) {
        return;
    }

    d->manager.reset();
    for (auto dd : std::as_const(WCursorImagePrivate::cursorImages)) {
        if (dd == d)
            continue;

        if (matchCursorTheme(dd->manager.get(), name, size)) {
            d->manager = dd->manager;
            break;
        }
    }
    if (!d->manager)
        d->manager.reset(qw_xcursor_manager::create(name.constData(), size));

    bool theme_loaded = d->manager->load(d->scale);
    if (!theme_loaded)
        qCCritical(qLcCursorImage) << "Can't load cursor theme:" << name << ", size:" << size;

    d->updateCursorImage();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wcursorimage.cpp"
