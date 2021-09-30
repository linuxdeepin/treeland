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
#include "wcursor.h"
#include "winputdevice.h"
#include "woutputlayout.h"
#include "wxcursormanager.h"
#include "wseat.h"
#include "woutput.h"
#include "wsignalconnector.h"

#include <QCursor>
#include <QPixmap>
#include <QQuickWindow>
#include <QCoreApplication>
#include <QDebug>

extern "C" {
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_pointer.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursorPrivate : public WObjectPrivate
{
public:
    WCursorPrivate(WCursor *qq)
        : WObjectPrivate(qq)
        , handle(wlr_cursor_create())
    {
        handle->data = qq;

        connect();
    }

    ~WCursorPrivate() {
        handle->data = nullptr;
        if (seat)
            seat->detachCursor(q_func());

        sc.invalidate();
        wlr_cursor_destroy(handle);
    }

    inline WOutput *outputAt(double x, double y) const
    {
        return outputLayout ? outputLayout->at(QPointF(x, y)) : nullptr;
    }

    // begin slot function
    void on_motion(void *data);
    void on_motion_absolute(void *data);
    void on_button(void *data);
    void on_axis(void *data);
    void on_frame(void *data);
    // end slot function

    void connect();
    void processCursorMotion(wlr_input_device *device, uint32_t time);

    W_DECLARE_PUBLIC(WCursor)

    wlr_cursor *handle;
    WXCursorManager *xcursor_manager = nullptr;
    WSeat *seat = nullptr;
    WOutputLayout *outputLayout = nullptr;
    WOutput *lastOutput = nullptr;
    QVector<WInputDevice*> deviceList;

    // for event data
    Qt::MouseButtons state = Qt::NoButton;
    Qt::MouseButton button = Qt::NoButton;
    QPointF lastPressedPosition;

    WSignalConnector sc;
};

void WCursorPrivate::on_motion(void *data)
{
    auto *event = reinterpret_cast<wlr_event_pointer_motion*>(data);

    wlr_cursor_move(handle, event->device,
                    event->delta_x, event->delta_y);
    processCursorMotion(event->device, event->time_msec);
}

void WCursorPrivate::on_motion_absolute(void *data)
{
    auto event = reinterpret_cast<wlr_event_pointer_motion_absolute*>(data);

    wlr_cursor_warp_absolute(handle, event->device, event->x, event->y);
    processCursorMotion(event->device, event->time_msec);
}

void WCursorPrivate::on_button(void *data)
{
    auto event = reinterpret_cast<wlr_event_pointer_button*>(data);
    button = WCursor::fromNativeButton(event->button);

    if (event->state == WLR_BUTTON_RELEASED) {
        state &= ~button;
    } else {
        state |= button;
        lastPressedPosition = q_func()->position();
    }

    if (Q_LIKELY(seat)) {
        seat->notifyButton(q_func(), WInputDevice::fromHandle<wlr_input_device>(event->device),
                           event->button, static_cast<WInputDevice::ButtonState>(event->state),
                           event->time_msec);
    }
}

void WCursorPrivate::on_axis(void *data)
{
    auto event = reinterpret_cast<wlr_event_pointer_axis*>(data);

    if (Q_LIKELY(seat)) {
        seat->notifyAxis(q_func(), WInputDevice::fromHandle<wlr_input_device>(event->device),
                         static_cast<WInputDevice::AxisSource>(event->source),
                         event->orientation == WLR_AXIS_ORIENTATION_HORIZONTAL
                         ? Qt::Horizontal : Qt::Vertical, event->delta, event->delta_discrete,
                         event->time_msec);
    }
}

void WCursorPrivate::on_frame(void *)
{
    if (Q_LIKELY(seat)) {
        seat->notifyFrame(q_func());
    }

    if (Q_LIKELY(lastOutput)) {
        lastOutput->requestRenderCursor();
    }
}

void WCursorPrivate::connect()
{
    sc.connect(&handle->events.motion,
               this, &WCursorPrivate::on_motion);
    sc.connect(&handle->events.motion_absolute,
               this, &WCursorPrivate::on_motion_absolute);
    sc.connect(&handle->events.button,
               this, &WCursorPrivate::on_button);
    sc.connect(&handle->events.axis,
               this, &WCursorPrivate::on_axis);
    sc.connect(&handle->events.frame,
               this, &WCursorPrivate::on_frame);
}

void WCursorPrivate::processCursorMotion(wlr_input_device *device, uint32_t time)
{
    W_Q(WCursor);
    auto output = outputAt(handle->x, handle->y);
    q->mapToOutput(output);

    if (Q_LIKELY(seat))
        seat->notifyMotion(q, WInputDevice::fromHandle<wlr_input_device>(device), time);
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
    auto wlr_handle = reinterpret_cast<const wlr_cursor*>(handle);
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
    d->seat = seat;
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
    wlr_xcursor_manager_set_cursor_image(d->xcursor_manager->nativeInterface<wlr_xcursor_manager>(),
                                         name, d->handle);
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
        wlr_cursor_set_image(d->handle, img.bits(), img.bytesPerLine(),
                             img.width(), img.height(), cursor.hotSpot().x(),
                             cursor.hotSpot().y(), d->lastOutput ? d->lastOutput->scale() : 1);
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
    wlr_cursor_attach_input_device(d->handle, device->nativeInterface<wlr_input_device>());

    if (d->lastOutput && d->lastOutput->isValid()) {
        wlr_cursor_map_input_to_output(d->handle, device->nativeInterface<wlr_input_device>(),
                                       d->lastOutput->nativeInterface<wlr_output>());
    }

    d->deviceList << device;
    return true;
}

void WCursor::detachInputDevice(WInputDevice *device)
{
    W_D(WCursor);

    if (!d->deviceList.removeOne(device))
        return;

    wlr_cursor_detach_input_device(d->handle, device->nativeInterface<wlr_input_device>());
    wlr_cursor_map_input_to_output(d->handle, device->nativeInterface<wlr_input_device>(), nullptr);
}

void WCursor::setOutputLayout(WOutputLayout *layout)
{
    W_D(WCursor);

    if (d->outputLayout == layout)
        return;

    if (d->outputLayout)
        d->outputLayout->removeCursor(this);

    d->outputLayout = layout;
    wlr_cursor_attach_output_layout(d->handle, layout ? layout->nativeInterface<wlr_output_layout>() : nullptr);

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

    auto output_handle = output ? output->nativeInterface<wlr_output>() : nullptr;
    wlr_cursor_map_to_output(d->handle, output_handle);

    Q_FOREACH(auto device, d->deviceList) {
        auto device_handle = device->nativeInterface<wlr_input_device>();
        wlr_cursor_map_input_to_output(d->handle, device_handle, output_handle);
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
    wlr_cursor_move(d->handle, nullptr, pos.x(), pos.y());
}

QPointF WCursor::position() const
{
    W_DC(WCursor);
    return QPointF(d->handle->x, d->handle->y);
}

QPointF WCursor::lastPressedPosition() const
{
    W_DC(WCursor);
    return d->lastPressedPosition;
}

WAYLIB_SERVER_END_NAMESPACE
