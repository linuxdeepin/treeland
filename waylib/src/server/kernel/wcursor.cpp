// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wcursor.h"
#include "private/wcursor_p.h"
#include "winputdevice.h"
#include "wxcursormanager.h"
#include "wxcursorimage.h"
#include "wseat.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "platformplugin/qwlrootsintegration.h"
#include "platformplugin/qwlrootscursor.h"

#include <qwcursor.h>
#include <qwoutput.h>
#include <qwxcursormanager.h>
#include <qwoutputlayout.h>
#include <qwinputdevice.h>
#include <qwpointer.h>
#include <qwsignalconnector.h>

#include <QCursor>
#include <QPixmap>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QDebug>

extern "C" {
#define static
#include <wlr/types/wlr_cursor.h>
#undef static
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_pointer.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WCursorPrivate::WCursorPrivate(WCursor *qq)
    : WObjectPrivate(qq)
    , handle(new QWCursor())
{
    handle->setData(this, qq);
}

WCursorPrivate::~WCursorPrivate()
{
    handle->setData(this, nullptr);
    if (seat)
        seat->setCursor(nullptr);

    if (outputLayout) {
        for (auto o : outputLayout->outputs())
            o->removeCursor(q_func());
    }

    clearCursorImages();

    delete handle;
}

void WCursorPrivate::setType(const char *name)
{
    W_Q(WCursor);

    if (!xcursor_manager)
        return;

    if (outputLayout) {
        for (auto o : outputLayout->outputs()) {
            xcursor_manager->load(o->scale());
            wlr_xcursor *xcursor = xcursor_manager->getXCursor(name, o->scale());
            cursorImages.append(new WXCursorImage(xcursor, o->scale()));
            // TODO: Support animation
            WXCursorImage *image = cursorImages.last();
            handle->setImage(image->image(), image->hotspot());
        }
    }
}

static inline const char *qcursorToType(const QCursor &cursor) {
    switch ((int)cursor.shape()) {
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
    default:
        break;
    }

    return nullptr;
}

void WCursorPrivate::updateCursorImage()
{
    clearCursorImages();

    if (auto type_name = qcursorToType(cursor)) {
        setType(type_name);
    } else {
        const QImage &img = cursor.pixmap().toImage();
        if (img.isNull())
            return;

        if (outputLayout) {
            for (auto o : outputLayout->outputs()) {
                QImage tmp = img;
                if (!qFuzzyCompare(img.devicePixelRatio(), static_cast<qreal>(o->scale()))) {
                    tmp = tmp.scaledToWidth(img.width() * o->scale() / img.devicePixelRatio(), Qt::SmoothTransformation);
                    tmp.setDevicePixelRatio(o->scale());
                }

                cursorImages.append(new WXCursorImage(tmp, cursor.hotSpot() * o->scale() / img.devicePixelRatio()));
                WXCursorImage *image = cursorImages.last();
                handle->setImage(image->image(), image->hotspot());
            }
        } else {
            cursorImages.append(new WXCursorImage(img, cursor.hotSpot()));
            handle->setImage(img, cursor.hotSpot());
        }
    }

    Q_EMIT q_func()->cursorImageMaybeChanged();
}

void WCursorPrivate::on_motion(wlr_pointer_motion_event *event)
{
    auto device = QWPointer::from(event->pointer);
    q_func()->move(device, QPointF(event->delta_x, event->delta_y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_motion_absolute(wlr_pointer_motion_absolute_event *event)
{
    auto device = QWPointer::from(event->pointer);
    q_func()->setScalePosition(device, QPointF(event->x, event->y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_button(wlr_pointer_button_event *event)
{
    auto device = QWPointer::from(event->pointer);
    button = WCursor::fromNativeButton(event->button);

    if (event->state == WLR_BUTTON_RELEASED) {
        state &= ~button;
    } else {
        state |= button;
        lastPressedPosition = q_func()->position();
    }

    if (Q_LIKELY(seat)) {
        seat->notifyButton(q_func(), WInputDevice::fromHandle<QWInputDevice>(device),
                           event->button, static_cast<WInputDevice::ButtonState>(event->state),
                           event->time_msec);
    }
}

void WCursorPrivate::on_axis(wlr_pointer_axis_event *event)
{
    auto device = QWPointer::from(event->pointer);

    if (Q_LIKELY(seat)) {
        seat->notifyAxis(q_func(), WInputDevice::fromHandle<QWInputDevice>(device),
                         static_cast<WInputDevice::AxisSource>(event->source),
                         event->orientation == WLR_AXIS_ORIENTATION_HORIZONTAL
                         ? Qt::Horizontal : Qt::Vertical, event->delta, event->delta_discrete,
                         event->time_msec);
    }
}

void WCursorPrivate::on_frame()
{
    if (Q_LIKELY(seat)) {
        seat->notifyFrame(q_func());
    }
}

void WCursorPrivate::connect()
{
    Q_ASSERT(seat);
    auto slotOwner = WServer::from(seat)->slotOwner();

    QObject::connect(handle, &QWCursor::motion, slotOwner, [this] (wlr_pointer_motion_event *event) {
        on_motion(event);
    });
    QObject::connect(handle, &QWCursor::motionAbsolute, slotOwner, [this] (wlr_pointer_motion_absolute_event *event) {
        on_motion_absolute(event);
    });
    QObject::connect(handle, &QWCursor::button, slotOwner, [this] (wlr_pointer_button_event *event) {
        on_button(event);
    });
    QObject::connect(handle, &QWCursor::axis, slotOwner, [this] (wlr_pointer_axis_event *event) {
        on_axis(event);
    });
    QObject::connect(handle, &QWCursor::frame, slotOwner, [this] () {
        on_frame();
    });
}

void WCursorPrivate::processCursorMotion(QWPointer *device, uint32_t time)
{
    W_Q(WCursor);

    if (Q_LIKELY(seat))
        seat->notifyMotion(q, WInputDevice::fromHandle<QWInputDevice>(device), time);
}

void WCursorPrivate::clearCursorImages()
{
    qDeleteAll(cursorImages);
    cursorImages.clear();
}

WCursor::WCursor(WCursorPrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd)
{

}

void WCursor::move(QWInputDevice *device, const QPointF &delta)
{
    d_func()->handle->move(device, delta);
}

void WCursor::setPosition(QWInputDevice *device, const QPointF &pos)
{
    d_func()->handle->warpClosest(device, pos);
}

bool WCursor::setPositionWithChecker(QWInputDevice *device, const QPointF &pos)
{
    return d_func()->handle->warp(device, pos);
}

void WCursor::setScalePosition(QWInputDevice *device, const QPointF &ratio)
{
    d_func()->handle->warpAbsolute(device, ratio);
}

WCursor::WCursor(QObject *parent)
    : WCursor(*new WCursorPrivate(this), parent)
{

}

WCursorHandle *WCursor::handle() const
{
    W_DC(WCursor);
    return reinterpret_cast<WCursorHandle*>(d->handle);
}

WCursor *WCursor::fromHandle(const WCursorHandle *handle)
{
    return reinterpret_cast<const QWCursor*>(handle)->getData<WCursor>();
}

Qt::MouseButton WCursor::fromNativeButton(uint32_t code)
{
    Qt::MouseButton qt_button = Qt::NoButton;
    // translate from kernel (input.h) 'button' to corresponding Qt:MouseButton.
    // The range of mouse values is 0x110 <= mouse_button < 0x120, the first Joystick button.
    switch (code) {
    case 0x110: qt_button = Qt::LeftButton; break;    // kernel BTN_LEFT
    case 0x111: qt_button = Qt::RightButton; break;
    case 0x112: qt_button = Qt::MiddleButton; break;
    case 0x113: qt_button = Qt::ExtraButton1; break;  // AKA Qt::BackButton
    case 0x114: qt_button = Qt::ExtraButton2; break;  // AKA Qt::ForwardButton
    case 0x115: qt_button = Qt::ExtraButton3; break;  // AKA Qt::TaskButton
    case 0x116: qt_button = Qt::ExtraButton4; break;
    case 0x117: qt_button = Qt::ExtraButton5; break;
    case 0x118: qt_button = Qt::ExtraButton6; break;
    case 0x119: qt_button = Qt::ExtraButton7; break;
    case 0x11a: qt_button = Qt::ExtraButton8; break;
    case 0x11b: qt_button = Qt::ExtraButton9; break;
    case 0x11c: qt_button = Qt::ExtraButton10; break;
    case 0x11d: qt_button = Qt::ExtraButton11; break;
    case 0x11e: qt_button = Qt::ExtraButton12; break;
    case 0x11f: qt_button = Qt::ExtraButton13; break;
    default: qWarning() << "invalid button number (as far as Qt is concerned)" << code; // invalid button number (as far as Qt is concerned)
    }

    return qt_button;
}

Qt::MouseButtons WCursor::state() const
{
    W_DC(WCursor);
    return d->state;
}

Qt::MouseButton WCursor::button() const
{
    W_DC(WCursor);
    return d->button;
}

void WCursor::setSeat(WSeat *seat)
{
    W_D(WCursor);

    if (d->seat) {
        // reconnect signals
        d->handle->disconnect(WServer::from(d->seat)->slotOwner());
    }
    d->seat = seat;

    if (d->seat) {
        d->connect();
    }
}

WSeat *WCursor::seat() const
{
    W_DC(WCursor);
    return d->seat;
}

QQuickWindow *WCursor::eventWindow() const
{
    W_DC(WCursor);
    return d->eventWindow.get();
}

void WCursor::setEventWindow(QQuickWindow *window)
{
    W_D(WCursor);
    d->eventWindow = window;
}

Qt::CursorShape WCursor::defaultCursor()
{
    return Qt::ArrowCursor;
}

void WCursor::setXCursorManager(QWXCursorManager *manager)
{
    W_D(WCursor);

    if (d->xcursor_manager == manager)
        return;

    d->xcursor_manager = manager;
    d->updateCursorImage();
}

void WCursor::setCursor(const QCursor &cursor)
{
    W_D(WCursor);

    d->cursor = cursor;
    d->updateCursorImage();
}

WXCursorImage *WCursor::getCursorImage(float scale) const
{
    W_DC(WCursor);

    if (d->cursorImages.isEmpty())
        return nullptr;

    WXCursorImage *maxScaleImage = nullptr;

    for (auto i : d->cursorImages) {
        if (i->scale() < scale) {
            if (!maxScaleImage || maxScaleImage->scale() < i->scale())
                maxScaleImage = i;
        } else {
            return i;
        }
    }

    return maxScaleImage;
}

bool WCursor::attachInputDevice(WInputDevice *device)
{
    if (device->type() != WInputDevice::Type::Pointer
            && device->type() != WInputDevice::Type::Touch
            && device->type() != WInputDevice::Type::Tablet) {
        return false;
    }

    W_D(WCursor);
    Q_ASSERT(!d->deviceList.contains(device));
    d->handle->attachInputDevice(device->nativeInterface<QWInputDevice>());
    d->deviceList << device;
    return true;
}

void WCursor::detachInputDevice(WInputDevice *device)
{
    W_D(WCursor);

    if (!d->deviceList.removeOne(device))
        return;

    d->handle->detachInputDevice(device->nativeInterface<QWInputDevice>());
    d->handle->mapInputToOutput(device->nativeInterface<QWInputDevice>(), nullptr);
}

void WCursor::setLayout(WOutputLayout *layout)
{
    W_D(WCursor);

    if (d->outputLayout == layout)
        return;

    if (d->outputLayout) {
        for (auto o : d->outputLayout->outputs())
            disconnect(o, SIGNAL(scaleChanged()), this, SLOT(updateCursorImage()));
    }

    d->outputLayout = layout;
    d->handle->attachOutputLayout(d->outputLayout);

    if (d->outputLayout) {
        for (auto o : d->outputLayout->outputs()) {
            connect(o, SIGNAL(scaleChanged()), this, SLOT(updateCursorImage()));
            o->addCursor(this);
        }
    }

    connect(d->outputLayout, &WOutputLayout::outputAdded, this, [this, d] (WOutput *o) {
        connect(o, SIGNAL(scaleChanged()), this, SLOT(updateCursorImage()));
        o->addCursor(this);
        d->updateCursorImage();
    });

    d->updateCursorImage();
}

WOutputLayout *WCursor::layout() const
{
    W_DC(WCursor);
    return d->outputLayout;
}

void WCursor::setPosition(const QPointF &pos)
{
    setPosition(nullptr, pos);
}

bool WCursor::setPositionWithChecker(const QPointF &pos)
{
    return setPositionWithChecker(nullptr, pos);
}

QPointF WCursor::position() const
{
    W_DC(WCursor);
    return d->handle->position();
}

QPointF WCursor::lastPressedPosition() const
{
    W_DC(WCursor);
    return d->lastPressedPosition;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wcursor.cpp"
