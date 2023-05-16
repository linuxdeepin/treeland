/*
 * Copyright (C) 2021 ~ 2022 zkyd
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
#include "wcursor.h"
#include "winputdevice.h"
#include "woutputlayout.h"
#include "wxcursormanager.h"
#include "wseat.h"
#include "woutput.h"
#include "wsignalconnector.h"

#include <qwcursor.h>
#include <qwoutput.h>
#include <qwxcursormanager.h>

#include <QCursor>
#include <QPixmap>
#include <QQuickWindow>
#include <QCoreApplication>
#include <QDebug>
#include <qwinputdevice.h>
#include <qwpointer.h>

extern "C" {
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_pointer.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursorPrivate : public WObjectPrivate
{
public:
    WCursorPrivate(WCursor *qq)
        : WObjectPrivate(qq)
        , handle(new QWCursor())
    {
        handle->handle()->data = qq;
    }

    ~WCursorPrivate() {
        handle->handle()->data = nullptr;
        if (seat)
            seat->detachCursor(q_func());

        delete handle;
    }

    inline WOutput *outputAt(const QPointF &pos) const
    {
        return outputLayout ? outputLayout->at(pos) : nullptr;
    }

    // begin slot function
    void on_motion(wlr_pointer_motion_event *event);
    void on_motion_absolute(wlr_pointer_motion_absolute_event *event);
    void on_button(wlr_pointer_button_event *event);
    void on_axis(wlr_pointer_axis_event *event);
    void on_frame();
    // end slot function

    void connect();
    void processCursorMotion(QWPointer *device, uint32_t time);

    W_DECLARE_PUBLIC(WCursor)

    QWCursor *handle;
    WXCursorManager *xcursor_manager = nullptr;
    WSeat *seat = nullptr;
    WOutputLayout *outputLayout = nullptr;
    WOutput *lastOutput = nullptr;
    QVector<WInputDevice*> deviceList;

    // for event data
    Qt::MouseButtons state = Qt::NoButton;
    Qt::MouseButton button = Qt::NoButton;
    QPointF lastPressedPosition;
};

void WCursorPrivate::on_motion(wlr_pointer_motion_event *event)
{
    auto device = QWPointer::from(event->pointer);
    handle->move(device, QPointF(event->delta_x, event->delta_y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_motion_absolute(wlr_pointer_motion_absolute_event *event)
{
    auto device = QWPointer::from(event->pointer);
    handle->warpAbsolute(device, QPointF(event->x, event->y));
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

#ifdef WAYLIB_DISABLE_OUTPUT_DAMAGE
    if (Q_LIKELY(lastOutput)) {
        lastOutput->requestRender();
    }
#endif
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
    auto output = outputAt(handle->position());
    q->mapToOutput(output);

    if (Q_LIKELY(seat))
        seat->notifyMotion(q, WInputDevice::fromHandle<QWInputDevice>(device), time);
}

WCursor::WCursor()
    : WObject(*new  WCursorPrivate(this))
{

}

WCursorHandle *WCursor::handle() const
{
    W_DC(WCursor);
    return reinterpret_cast<WCursorHandle*>(d->handle);
}

WCursor *WCursor::fromHandle(const WCursorHandle *handle)
{
    auto wlr_handle = reinterpret_cast<const QWCursor*>(handle)->handle();
    return reinterpret_cast<WCursor*>(wlr_handle->data);
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

Qt::CursorShape WCursor::defaultCursor()
{
    return Qt::ArrowCursor;
}

void WCursor::setManager(WXCursorManager *manager)
{
    W_D(WCursor);
    d->xcursor_manager = manager;
}

void WCursor::setType(const char *name)
{
    W_D(WCursor);
    Q_ASSERT(d->xcursor_manager);
    if (d->lastOutput) {
        bool ok = d->xcursor_manager->load(d->lastOutput->scale());
        Q_ASSERT(ok);
    }
    d->xcursor_manager->nativeInterface<QWXCursorManager>()->setCursor(name, d->handle);
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

void WCursor::setCursor(const QCursor &cursor)
{
    if (auto type_name = qcursorToType(cursor)) {
        setType(type_name);
    } else {
        const QImage &img = cursor.pixmap().toImage();

        if (img.isNull())
            return;

        W_D(WCursor);
        d->handle->setImage(img, cursor.hotSpot());
    }
}

void WCursor::reset()
{
    setCursor(mappedOutput()->cursor());
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

    if (d->lastOutput && d->lastOutput->isValid()) {
        d->handle->mapInputToOutput(device->nativeInterface<QWInputDevice>(),
                                    d->lastOutput->nativeInterface<QWOutput>()->handle());
    }

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

void WCursor::setOutputLayout(WOutputLayout *layout)
{
    W_D(WCursor);

    if (d->outputLayout == layout)
        return;

    if (d->outputLayout)
        d->outputLayout->removeCursor(this);

    d->outputLayout = layout;
    d->handle->attachOutputLayout(layout->nativeInterface<QWOutputLayout>());

    if (layout)
        layout->addCursor(this);
}

void WCursor::mapToOutput(WOutput *output)
{
    W_D(WCursor);

    bool output_changed = d->lastOutput != output;

    if (Q_UNLIKELY(!output_changed))
        return;

    if (d->lastOutput) {
        d->lastOutput->cursorLeave(this);
    }

    d->lastOutput = output;

    if (output) {
        output->cursorEnter(this);
    }

    auto output_handle = output ? output->nativeInterface<QWOutput>()->handle() : nullptr;
    d->handle->mapToOutput(output_handle);

    Q_FOREACH(auto device, d->deviceList) {
        auto device_handle = device->nativeInterface<QWInputDevice>();
        d->handle->mapInputToOutput(device_handle, output_handle);
    }
}

WOutput *WCursor::mappedOutput() const
{
    W_DC(WCursor);
    return d->lastOutput;
}

void WCursor::setPosition(const QPointF &pos)
{
    W_D(WCursor);
    d->handle->move(nullptr, pos);
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
