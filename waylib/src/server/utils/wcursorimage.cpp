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

// This function copy form kwin 6.1.19 `src/cursor.cpp`
// Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org> GPL-2.0-or-later
static QList<QByteArray> alternativesCursorShape(const QByteArray &name)
{
    static const QHash<QByteArray, QList<QByteArray>> alternatives = {
        {
            QByteArrayLiteral("crosshair"),
            {
                QByteArrayLiteral("cross"),
                QByteArrayLiteral("diamond-cross"),
                QByteArrayLiteral("cross-reverse"),
            },
        },
        {
            QByteArrayLiteral("default"),
            {
                QByteArrayLiteral("left_ptr"),
                QByteArrayLiteral("arrow"),
                QByteArrayLiteral("dnd-none"),
                QByteArrayLiteral("op_left_arrow"),
            },
        },
        {
            QByteArrayLiteral("up-arrow"),
            {
                QByteArrayLiteral("up_arrow"),
                QByteArrayLiteral("sb_up_arrow"),
                QByteArrayLiteral("center_ptr"),
                QByteArrayLiteral("centre_ptr"),
            },
        },
        {
            QByteArrayLiteral("wait"),
            {
                QByteArrayLiteral("watch"),
                QByteArrayLiteral("progress"),
            },
        },
        {
            QByteArrayLiteral("text"),
            {
                QByteArrayLiteral("ibeam"),
                QByteArrayLiteral("xterm"),
            },
        },
        {
            QByteArrayLiteral("all-scroll"),
            {
                QByteArrayLiteral("size_all"),
                QByteArrayLiteral("fleur"),
            },
        },
        {
            QByteArrayLiteral("pointer"),
            {
                QByteArrayLiteral("pointing_hand"),
                QByteArrayLiteral("hand2"),
                QByteArrayLiteral("hand"),
                QByteArrayLiteral("hand1"),
                QByteArrayLiteral("e29285e634086352946a0e7090d73106"),
                QByteArrayLiteral("9d800788f1b08800ae810202380a0822"),
            },
        },
        {
            QByteArrayLiteral("ns-resize"),
            {
                QByteArrayLiteral("size_ver"),
                QByteArrayLiteral("00008160000006810000408080010102"),
                QByteArrayLiteral("sb_v_double_arrow"),
                QByteArrayLiteral("v_double_arrow"),
                QByteArrayLiteral("n-resize"),
                QByteArrayLiteral("s-resize"),
                QByteArrayLiteral("col-resize"),
                QByteArrayLiteral("top_side"),
                QByteArrayLiteral("bottom_side"),
                QByteArrayLiteral("base_arrow_up"),
                QByteArrayLiteral("base_arrow_down"),
                QByteArrayLiteral("based_arrow_down"),
                QByteArrayLiteral("based_arrow_up"),
            },
        },
        {
            QByteArrayLiteral("ew-resize"),
            {
                QByteArrayLiteral("size_hor"),
                QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                QByteArrayLiteral("sb_h_double_arrow"),
                QByteArrayLiteral("h_double_arrow"),
                QByteArrayLiteral("e-resize"),
                QByteArrayLiteral("w-resize"),
                QByteArrayLiteral("row-resize"),
                QByteArrayLiteral("right_side"),
                QByteArrayLiteral("left_side"),
            },
        },
        {
            QByteArrayLiteral("nesw-resize"),
            {
                QByteArrayLiteral("size_bdiag"),
                QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                QByteArrayLiteral("fd_double_arrow"),
                QByteArrayLiteral("bottom_left_corner"),
                QByteArrayLiteral("top_right_corner"),
            },
        },
        {
            QByteArrayLiteral("nwse-resize"),
            {
                QByteArrayLiteral("size_fdiag"),
                QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                QByteArrayLiteral("bd_double_arrow"),
                QByteArrayLiteral("bottom_right_corner"),
                QByteArrayLiteral("top_left_corner"),
            },
        },
        {
            QByteArrayLiteral("help"),
            {
                QByteArrayLiteral("whats_this"),
                QByteArrayLiteral("d9ce0ab605698f320427677b458ad60b"),
                QByteArrayLiteral("left_ptr_help"),
                QByteArrayLiteral("question_arrow"),
                QByteArrayLiteral("dnd-ask"),
                QByteArrayLiteral("5c6cd98b3f3ebcb1f9c7f1c204630408"),
            },
        },
        {
            QByteArrayLiteral("col-resize"),
            {
                QByteArrayLiteral("split_h"),
                QByteArrayLiteral("14fef782d02440884392942c11205230"),
                QByteArrayLiteral("size_hor"),
            },
        },
        {
            QByteArrayLiteral("row-resize"),
            {
                QByteArrayLiteral("split_v"),
                QByteArrayLiteral("2870a09082c103050810ffdffffe0204"),
                QByteArrayLiteral("size_ver"),
            },
        },
        {
            QByteArrayLiteral("not-allowed"),
            {
                QByteArrayLiteral("forbidden"),
                QByteArrayLiteral("03b6e0fcb3499374a867c041f52298f0"),
                QByteArrayLiteral("circle"),
                QByteArrayLiteral("dnd-no-drop"),
            },
        },
        {
            QByteArrayLiteral("progress"),
            {
                QByteArrayLiteral("left_ptr_watch"),
                QByteArrayLiteral("3ecb610c1bf2410f44200f48c40d3599"),
                QByteArrayLiteral("00000000000000020006000e7e9ffc3f"),
                QByteArrayLiteral("08e8e1c95fe2fc01f976f1e063a24ccd"),
            },
        },
        {
            QByteArrayLiteral("grab"),
            {
                QByteArrayLiteral("openhand"),
                QByteArrayLiteral("9141b49c8149039304290b508d208c40"),
                QByteArrayLiteral("all_scroll"),
                QByteArrayLiteral("all-scroll"),
            },
        },
        {
            QByteArrayLiteral("grabbing"),
            {
                QByteArrayLiteral("closedhand"),
                QByteArrayLiteral("05e88622050804100c20044008402080"),
                QByteArrayLiteral("4498f0e0c1937ffe01fd06f973665830"),
                QByteArrayLiteral("9081237383d90e509aa00f00170e968f"),
                QByteArrayLiteral("fcf21c00b30f7e3f83fe0dfd12e71cff"),
            },
        },
        {
            QByteArrayLiteral("alias"),
            {
                QByteArrayLiteral("link"),
                QByteArrayLiteral("dnd-link"),
                QByteArrayLiteral("3085a0e285430894940527032f8b26df"),
                QByteArrayLiteral("640fb0e74195791501fd1ed57b41487f"),
                QByteArrayLiteral("a2a266d0498c3104214a47bd64ab0fc8"),
            },
        },
        {
            QByteArrayLiteral("copy"),
            {
                QByteArrayLiteral("dnd-copy"),
                QByteArrayLiteral("1081e37283d90000800003c07f3ef6bf"),
                QByteArrayLiteral("6407b0e94181790501fd1e167b474872"),
                QByteArrayLiteral("b66166c04f8c3109214a4fbd64a50fc8"),
            },
        },
        {
            QByteArrayLiteral("move"),
            {
                QByteArrayLiteral("dnd-move"),
            },
        },
        {
            QByteArrayLiteral("sw-resize"),
            {
                QByteArrayLiteral("size_bdiag"),
                QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                QByteArrayLiteral("fd_double_arrow"),
                QByteArrayLiteral("bottom_left_corner"),
            },
        },
        {
            QByteArrayLiteral("se-resize"),
            {
                QByteArrayLiteral("size_fdiag"),
                QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                QByteArrayLiteral("bd_double_arrow"),
                QByteArrayLiteral("bottom_right_corner"),
            },
        },
        {
            QByteArrayLiteral("ne-resize"),
            {
                QByteArrayLiteral("size_bdiag"),
                QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                QByteArrayLiteral("fd_double_arrow"),
                QByteArrayLiteral("top_right_corner"),
            },
        },
        {
            QByteArrayLiteral("nw-resize"),
            {
                QByteArrayLiteral("size_fdiag"),
                QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                QByteArrayLiteral("bd_double_arrow"),
                QByteArrayLiteral("top_left_corner"),
            },
        },
        {
            QByteArrayLiteral("n-resize"),
            {
                QByteArrayLiteral("size_ver"),
                QByteArrayLiteral("00008160000006810000408080010102"),
                QByteArrayLiteral("sb_v_double_arrow"),
                QByteArrayLiteral("v_double_arrow"),
                QByteArrayLiteral("col-resize"),
                QByteArrayLiteral("top_side"),
            },
        },
        {
            QByteArrayLiteral("e-resize"),
            {
                QByteArrayLiteral("size_hor"),
                QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                QByteArrayLiteral("sb_h_double_arrow"),
                QByteArrayLiteral("h_double_arrow"),
                QByteArrayLiteral("row-resize"),
                QByteArrayLiteral("left_side"),
            },
        },
        {
            QByteArrayLiteral("s-resize"),
            {
                QByteArrayLiteral("size_ver"),
                QByteArrayLiteral("00008160000006810000408080010102"),
                QByteArrayLiteral("sb_v_double_arrow"),
                QByteArrayLiteral("v_double_arrow"),
                QByteArrayLiteral("col-resize"),
                QByteArrayLiteral("bottom_side"),
            },
        },
        {
            QByteArrayLiteral("w-resize"),
            {
                QByteArrayLiteral("size_hor"),
                QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                QByteArrayLiteral("sb_h_double_arrow"),
                QByteArrayLiteral("h_double_arrow"),
                QByteArrayLiteral("right_side"),
            },
        },
        {
            QByteArrayLiteral("ibeam"),
            {
                QByteArrayLiteral("text"),
                QByteArrayLiteral("xterm"),
            },
        },
    };
    auto it = alternatives.find(name);
    if (it != alternatives.end()) {
        return it.value();
    }
    return QList<QByteArray>();
}

static inline wlr_xcursor *getXCursorWithFallback(qw_xcursor_manager *xcursor_manager, const char *name, float scale)
{
    if (!xcursor_manager)
        return nullptr;

    if (wlr_xcursor *cursor = xcursor_manager->get_xcursor(name, scale)) {
        return cursor;
    }

    const auto alterNameList = alternativesCursorShape(name);

    for (const auto &alterName : alterNameList) {
        if (auto cursor = xcursor_manager->get_xcursor(alterName, scale))
            return cursor;
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
    case static_cast<int>(WGlobal::CursorShape::DndAsk):
        return "dnd-ask";
    case static_cast<int>(WGlobal::CursorShape::AllResize):
        return "all-resize";
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

    QImage image;// TODO: supports multi threads
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
    Q_ASSERT(static_cast<unsigned int>(currentXCursorImageIndex) < xcursor->image_count);
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
