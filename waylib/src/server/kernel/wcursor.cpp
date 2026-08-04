// Copyright (C) 2023 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>
#include <QCursor>
#include "private/wprivateaccessor_p.h"

#include "wcursor.h"
#include "private/wcursor_p.h"
#include "winputdevice.h"
#include "wimagebuffer.h"
#include "wseat.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wayliblogging.h"

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output_layout.h>

#include <QPixmap>
#include <QCoreApplication>
#include <QQuickWindow>
#include <private/qcursor_p.h>

W_DECLARE_PRIVATE_MEMBER(QCursor_d_tag, QCursor, d, QCursorData*);

WAYLIB_SERVER_BEGIN_NAMESPACE

WCursorPrivate::WCursorPrivate(WCursor *qq)
    : WWrapObjectPrivate(qq)
    , overrideCursor(WCursor::toQCursor(WGlobal::CursorShape::Invalid))
{
    auto *cursor = wlr_cursor_create();
    initNativeHandle(cursor, &cursor->events.destroy);
}

WCursorPrivate::~WCursorPrivate()
{

}

void WCursorPrivate::instantRelease()
{
    qCDebug(lcWlCursor) << "Releasing cursor" << q_func();

    removeAllListeners();

    if (seat) {
        qCDebug(lcWlCursor) << "Detaching cursor from seat:" << seat->name();
        seat->setCursor(nullptr);
    }

    if (outputLayout) {
        qCDebug(lcWlCursor) << "Removing cursor from" << outputLayout->outputs().size() << "outputs";
        for (auto o : outputLayout->outputs())
            o->removeCursor(q_func());
    }

    if (nativeHandle())
        wlr_cursor_destroy(nativeHandle());
}

void WCursorPrivate::sendEnterEvent(WInputDevice *device)
{
    W_Q(WCursor);
    Q_ASSERT(device->qtDevice());
    const QPointF global = q->position();
    const QPointF local = global - eventWindow->position();
    QEnterEvent event(local, local, global, device->qtDevice<QPointingDevice>());
    QCoreApplication::sendEvent(eventWindow, &event);
}

void WCursorPrivate::sendLeaveEvent(WInputDevice *device)
{
    Q_ASSERT(device->qtDevice());
    QInputEvent event(QEvent::Leave, device->qtDevice<QPointingDevice>());
    QCoreApplication::sendEvent(eventWindow, &event);
}

void WCursorPrivate::on_motion(wlr_pointer_motion_event *event)
{
    auto device = event->pointer;
    q_func()->move(&device->base, QPointF(event->delta_x, event->delta_y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_motion_absolute(wlr_pointer_motion_absolute_event *event)
{
    auto device = event->pointer;
    q_func()->setScalePosition(&device->base, QPointF(event->x, event->y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_button(wlr_pointer_button_event *event)
{
    auto device = event->pointer;
    button = WCursor::fromNativeButton(event->button);

    QString stateStr = (event->state == WL_POINTER_BUTTON_STATE_RELEASED) ? "released" : "pressed";
    qCDebug(lcWlPointer) << "Button" << static_cast<int>(button) << stateStr
                              << "at position:" << q_func()->position();

    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        state &= ~button;
    } else {
        state |= button;
        lastPressedOrTouchDownPosition = q_func()->position();
    }

    if (auto inputDevice = WInputDevice::fromHandle(&device->base)) {
        if (auto deviceSeat = inputDevice->seat()) {
            deviceSeat->notifyButton(q_func(), inputDevice, button, event->state, event->time_msec);
        }
    }
}

void WCursorPrivate::on_axis(wlr_pointer_axis_event *event)
{
    auto device = event->pointer;

    if (auto inputDevice = WInputDevice::fromHandle(&device->base)) {
        if (auto deviceSeat = inputDevice->seat()) {
            deviceSeat->notifyAxis(q_func(), inputDevice, event->source,
                                 event->orientation == WL_POINTER_AXIS_HORIZONTAL_SCROLL
                                 ? Qt::Horizontal : Qt::Vertical, event->relative_direction,
                                 event->delta * scrollFactor, event->delta_discrete, event->time_msec);
        }
    }
}

void WCursorPrivate::on_frame()
{
    if (Q_LIKELY(seat)) {
        seat->notifyFrame(q_func());
    }
}

void WCursorPrivate::on_swipe_begin(wlr_pointer_swipe_begin_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(&device->base),
                               event->time_msec, event->fingers, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_update(wlr_pointer_swipe_update_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(&device->base),
                                event->time_msec, delta, 0, 0, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_end(wlr_pointer_swipe_end_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(&device->base),
                             event->time_msec, event->cancelled, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_pinch_begin(wlr_pointer_pinch_begin_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(&device->base),
                              event->time_msec, event->fingers, WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_update(wlr_pointer_pinch_update_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(&device->base),
                                event->time_msec, delta, event->scale, event->rotation,
                                WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_end(wlr_pointer_pinch_end_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(&device->base),
                             event->time_msec, event->cancelled,
                             WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_hold_begin(wlr_pointer_hold_begin_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        seat->notifyHoldBegin(q_func(), WInputDevice::fromHandle(&device->base),
                              event->time_msec, event->fingers);
    }
}

void WCursorPrivate::on_hold_end(wlr_pointer_hold_end_event *event)
{
    auto device = event->pointer;
    if (Q_LIKELY(seat)) {
        seat->notifyHoldEnd(q_func(), WInputDevice::fromHandle(&device->base),
                            event->time_msec, event->cancelled);
    }
}

void WCursorPrivate::on_touch_down(wlr_touch_down_event *event)
{
    auto device = event->touch;

    q_func()->setScalePosition(&device->base, QPointF(event->x, event->y));
    lastPressedOrTouchDownPosition = q_func()->position();

    if (Q_LIKELY(seat)) {
        seat->notifyTouchDown(q_func(), WInputDevice::fromHandle(&device->base),
                              event->touch_id, event->time_msec);
    }

}

void WCursorPrivate::on_touch_motion(wlr_touch_motion_event *event)
{
    auto device = event->touch;

    q_func()->setScalePosition(&device->base, QPointF(event->x, event->y));

    if (Q_LIKELY(seat)) {
        seat->notifyTouchMotion(q_func(), WInputDevice::fromHandle(&device->base),
                                event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::on_touch_frame()
{
    if (Q_LIKELY(seat)) {
        seat->notifyTouchFrame(q_func());
    }
}

void WCursorPrivate::on_touch_cancel(wlr_touch_cancel_event *event)
{
    auto device = event->touch;

    if (Q_LIKELY(seat)) {
        seat->notifyTouchCancel(q_func(), WInputDevice::fromHandle(&device->base),
                                event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::on_touch_up(wlr_touch_up_event *event)
{
    auto device = event->touch;

    if (Q_LIKELY(seat)) {
        seat->notifyTouchUp(q_func(), WInputDevice::fromHandle(&device->base),
                            event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::connect()
{
    W_Q(WCursor);
    wlr_cursor *cursor = nativeHandle();

    m_motionListener.connect(&cursor->events.motion, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_motionListener>(listener);
        self->on_motion(static_cast<wlr_pointer_motion_event*>(data));
    });
    m_motionAbsoluteListener.connect(&cursor->events.motion_absolute, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_motionAbsoluteListener>(listener);
        self->on_motion_absolute(static_cast<wlr_pointer_motion_absolute_event*>(data));
    });
    m_buttonListener.connect(&cursor->events.button, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_buttonListener>(listener);
        self->on_button(static_cast<wlr_pointer_button_event*>(data));
    });
    m_axisListener.connect(&cursor->events.axis, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_axisListener>(listener);
        self->on_axis(static_cast<wlr_pointer_axis_event*>(data));
    });
    m_frameListener.connect(&cursor->events.frame, [](wl_listener *listener, void *) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_frameListener>(listener);
        self->on_frame();
    });
    m_swipeBeginListener.connect(&cursor->events.swipe_begin, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_swipeBeginListener>(listener);
        self->on_swipe_begin(static_cast<wlr_pointer_swipe_begin_event*>(data));
    });
    m_swipeUpdateListener.connect(&cursor->events.swipe_update, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_swipeUpdateListener>(listener);
        self->on_swipe_update(static_cast<wlr_pointer_swipe_update_event*>(data));
    });
    m_swipeEndListener.connect(&cursor->events.swipe_end, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_swipeEndListener>(listener);
        self->on_swipe_end(static_cast<wlr_pointer_swipe_end_event*>(data));
    });
    m_pinchBeginListener.connect(&cursor->events.pinch_begin, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_pinchBeginListener>(listener);
        self->on_pinch_begin(static_cast<wlr_pointer_pinch_begin_event*>(data));
    });
    m_pinchUpdateListener.connect(&cursor->events.pinch_update, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_pinchUpdateListener>(listener);
        self->on_pinch_update(static_cast<wlr_pointer_pinch_update_event*>(data));
    });
    m_pinchEndListener.connect(&cursor->events.pinch_end, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_pinchEndListener>(listener);
        self->on_pinch_end(static_cast<wlr_pointer_pinch_end_event*>(data));
    });
    m_holdBeginListener.connect(&cursor->events.hold_begin, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_holdBeginListener>(listener);
        self->on_hold_begin(static_cast<wlr_pointer_hold_begin_event*>(data));
    });
    m_holdEndListener.connect(&cursor->events.hold_end, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_holdEndListener>(listener);
        self->on_hold_end(static_cast<wlr_pointer_hold_end_event*>(data));
    });

    // Handle touch device related signals
    m_touchDownListener.connect(&cursor->events.touch_down, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_touchDownListener>(listener);
        self->on_touch_down(static_cast<wlr_touch_down_event*>(data));
    });
    m_touchMotionListener.connect(&cursor->events.touch_motion, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_touchMotionListener>(listener);
        self->on_touch_motion(static_cast<wlr_touch_motion_event*>(data));
    });
    m_touchFrameListener.connect(&cursor->events.touch_frame, [](wl_listener *listener, void *) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_touchFrameListener>(listener);
        self->on_touch_frame();
    });
    m_touchCancelListener.connect(&cursor->events.touch_cancel, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_touchCancelListener>(listener);
        self->on_touch_cancel(static_cast<wlr_touch_cancel_event*>(data));
    });
    m_touchUpListener.connect(&cursor->events.touch_up, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WCursorPrivate, &WCursorPrivate::m_touchUpListener>(listener);
        self->on_touch_up(static_cast<wlr_touch_up_event*>(data));
    });
}

void WCursorPrivate::removeAllListeners()
{
    m_motionListener.remove();
    m_motionAbsoluteListener.remove();
    m_buttonListener.remove();
    m_axisListener.remove();
    m_frameListener.remove();
    m_swipeBeginListener.remove();
    m_swipeUpdateListener.remove();
    m_swipeEndListener.remove();
    m_pinchBeginListener.remove();
    m_pinchUpdateListener.remove();
    m_pinchEndListener.remove();
    m_holdBeginListener.remove();
    m_holdEndListener.remove();
    m_touchDownListener.remove();
    m_touchMotionListener.remove();
    m_touchFrameListener.remove();
    m_touchCancelListener.remove();
    m_touchUpListener.remove();
}

void WCursorPrivate::processCursorMotion(wlr_pointer *device, uint32_t time)
{
    W_Q(WCursor);

    qCDebug(lcWlPointer) << "Processing cursor motion at" << q->position()
                              << "time:" << time;

    if (auto inputDevice = WInputDevice::fromHandle(&device->base)) {
        if (auto deviceSeat = inputDevice->seat()) {
            deviceSeat->notifyMotion(q, inputDevice, time);
        }
    }
}

WCursor::WCursor(WCursorPrivate &dd, QObject *parent)
    : WWrapObject(dd, parent)
{

}

void WCursor::move(wlr_input_device *device, const QPointF &delta)
{
    const QPointF oldPos = position();
    wlr_cursor_move(d_func()->handle(), device, delta.x(), delta.y());

    if (oldPos != position()) {
        qCDebug(lcWlCursor) << "Cursor moved from" << oldPos << "to" << position()
                             << "delta:" << delta;
        Q_EMIT positionChanged();
    }
}

void WCursor::setPosition(wlr_input_device *device, const QPointF &pos)
{
    const QPointF oldPos = position();
    wlr_cursor_warp_closest(d_func()->handle(), device, pos.x(), pos.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
}

bool WCursor::setPositionWithChecker(wlr_input_device *device, const QPointF &pos)
{
    const QPointF oldPos = position();
    bool ok = wlr_cursor_warp(d_func()->handle(), device, pos.x(), pos.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
    return ok;
}

void WCursor::setScalePosition(wlr_input_device *device, const QPointF &ratio)
{
    Q_ASSERT(layout());
    const QPointF oldPos = position();
    wlr_cursor_warp_absolute(d_func()->handle(), device, ratio.x(), ratio.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
}

WCursor::WCursor(QObject *parent)
    : WCursor(*new WCursorPrivate(this), parent)
{

}

wlr_cursor *WCursor::handle() const
{
    W_DC(WCursor);
    return d->handle();
}

WCursor *WCursor::fromHandle(const wlr_cursor *handle)
{
    return static_cast<WCursor*>(WWrapObjectPrivate::fromNativeHandle(handle));
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
    default: 
        qCWarning(lcWlPointer) << "Invalid button code:" << QString("0x%1").arg(code, 0, 16)
                                    << "- not mappable to Qt button";
    }

    return qt_button;
}

uint32_t WCursor::toNativeButton(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return 0x110;    // kernel BTN_LEFT
    case Qt::RightButton: return 0x111;
    case Qt::MiddleButton: return 0x112;
    case Qt::ExtraButton1: return 0x113;
    case Qt::ExtraButton2: return 0x114;
    case Qt::ExtraButton3: return 0x115;
    case Qt::ExtraButton4: return 0x116;
    case Qt::ExtraButton5: return 0x117;
    case Qt::ExtraButton6: return 0x118;
    case Qt::ExtraButton7: return 0x119;
    case Qt::ExtraButton8: return 0x11a;
    case Qt::ExtraButton9: return 0x11b;
    case Qt::ExtraButton10: return 0x11c;
    case Qt::ExtraButton11: return 0x11d;
    case Qt::ExtraButton12: return 0x11e;
    case Qt::ExtraButton13: return 0x11f;
    default:
        qCWarning(lcWlPointer) << "Invalid Qt button:" << button 
                                    << "- cannot be mapped to native button code";
    }

    return 0;
}

QCursor WCursor::toQCursor(CursorShape shape)
{
    static QBitmap tmp(1, 1);
    // Ensure alloc a new QCursorData
    QCursor cursor(tmp, tmp);

    Q_ASSERT(W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->ref == 1);
    Q_ASSERT(W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->bm);
    Q_ASSERT(W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->bmm);
    delete W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->bm;
    delete W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->bmm;
    W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->bm = nullptr;
    W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->bmm = nullptr;
    W_PRIVATE_MEMBER(cursor, QCursor_d_tag{})->cshape = static_cast<Qt::CursorShape>(shape);

    return cursor;
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
        d->removeAllListeners();
        d->seat->disconnect(this);
    }
    d->seat = seat;

    if (d->seat) {
        d->connect();

        connect(d->seat, &WSeat::requestCursorShape, this, &WCursor::requestedCursorShapeChanged);
        connect(d->seat, &WSeat::requestCursorSurface, this, &WCursor::requestedCursorSurfaceChanged);
        connect(d->seat, &WSeat::requestDrag, this, &WCursor::requestedDragSurfaceChanged);
    }

    Q_EMIT seatChanged();
    Q_EMIT requestedCursorShapeChanged();
    Q_EMIT requestedCursorSurfaceChanged();
    Q_EMIT requestedDragSurfaceChanged();
}

WSeat *WCursor::seat() const
{
    W_DC(WCursor);
    return d->seat;
}

QWindow *WCursor::eventWindow() const
{
    W_DC(WCursor);
    return d->eventWindow.get();
}

void WCursor::setEventWindow(QWindow *window)
{
    W_D(WCursor);
    if (d->eventWindow == window)
        return;

    if (d->eventWindow && d->seat) {
        for (auto device : std::as_const(d->deviceList)) {
            d->sendLeaveEvent(device);
        }
    }

    d->eventWindow = window;

    if (d->eventWindow && d->seat) {
        for (auto device : std::as_const(d->deviceList)) {
            d->sendEnterEvent(device);
        }
    }
}

Qt::CursorShape WCursor::defaultCursor()
{
    return Qt::ArrowCursor;
}

QCursor WCursor::cursor() const
{
    W_DC(WCursor);
    return WGlobal::isInvalidCursor(d->overrideCursor)
            ? d->cursor
            : d->overrideCursor;
}

void WCursor::setCursor(const QCursor &cursor)
{
    W_D(WCursor);

    if (d->cursor == cursor)
        return;

    d->cursor = cursor;
    if (WGlobal::isInvalidCursor(d->overrideCursor))
        Q_EMIT cursorChanged();
}

QCursor WCursor::overrideCursor() const
{
    W_DC(WCursor);
    return d->overrideCursor;
}

void WCursor::setOverrideCursor(const QCursor &cursor)
{
    W_D(WCursor);

    if (d->overrideCursor == cursor)
        return;

    d->overrideCursor = cursor;
    Q_EMIT cursorChanged();
}

WGlobal::CursorShape WCursor::requestedCursorShape() const
{
    W_DC(WCursor);
    return d->seat ? d->seat->requestedCursorShape() : WGlobal::CursorShape::Invalid;
}

std::pair<WSurface *, QPoint> WCursor::requestedCursorSurface() const
{
    W_DC(WCursor);
    if (!d->seat)
        return {};

    return std::make_pair(d->seat->requestedCursorSurface(),
                          d->seat->requestedCursorSurfaceHotspot());
}

WSurface *WCursor::requestedDragSurface() const
{
    W_DC(WCursor);
    return d->seat ? d->seat->requestedDragSurface() : nullptr;
}

bool WCursor::attachInputDevice(WInputDevice *device)
{
    if (device->type() != WInputDevice::Type::Pointer
            && device->type() != WInputDevice::Type::Touch
            && device->type() != WInputDevice::Type::Tablet) {
        qCDebug(lcWlCursor) << "Cannot attach device type" << static_cast<int>(device->type())
                             << "to cursor - not a pointing device";
        return false;
    }

    W_D(WCursor);
    Q_ASSERT(!d->deviceList.contains(device));
    qCDebug(lcWlCursor) << "Attaching input device" << device->qtDevice()->name() 
                         << "of type" << static_cast<int>(device->type()) << "to cursor";
    wlr_cursor_attach_input_device(d->handle(), device->handle());
    d->deviceList << device;

    if (d->eventWindow) {
        Q_ASSERT(d->seat);
        d->sendEnterEvent(device);
    }

    return true;
}

void WCursor::detachInputDevice(WInputDevice *device)
{
    W_D(WCursor);

    if (!d->deviceList.removeOne(device)) {
        qCDebug(lcWlCursor) << "Cannot detach device" << device->qtDevice()->name()
                             << "- not attached to this cursor";
        return;
    }

    qCDebug(lcWlCursor) << "Detaching input device" << device->qtDevice()->name() 
                         << "from cursor";
    wlr_cursor_detach_input_device(d->handle(), device->handle());
    wlr_cursor_map_input_to_output(d->handle(), device->handle(), nullptr);

    if (d->eventWindow && device->seat()) {
        Q_ASSERT(d->seat);
        d->sendLeaveEvent(device);
    }
}

void WCursor::setLayout(WOutputLayout *layout)
{
    W_D(WCursor);

    if (d->outputLayout == layout)
        return;

    d->outputLayout = layout;
    wlr_cursor_attach_output_layout(d->handle(), d->outputLayout->handle());

    if (d->outputLayout) {
        for (auto o : d->outputLayout->outputs())
            o->addCursor(this);
    }

    connect(d->outputLayout, &WOutputLayout::outputAdded, this, [this] (WOutput *o) {
        o->addCursor(this);
    });

    connect(d->outputLayout, &WOutputLayout::outputRemoved, this, [this] (WOutput *o) {
        o->removeCursor(this);
    });

    Q_EMIT layoutChanged();
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

bool WCursor::isVisible() const
{
    W_DC(WCursor);
    return d->visible;
}

void WCursor::setVisible(bool visible)
{
    W_D(WCursor);
    if (d->visible == visible)
        return;
    d->visible = visible;
    Q_EMIT visibleChanged();
}

QPointF WCursor::position() const
{
    W_DC(WCursor);
    return QPointF(d->nativeHandle()->x, d->nativeHandle()->y);
}

QPointF WCursor::lastPressedOrTouchDownPosition() const
{
    W_DC(WCursor);
    return d->lastPressedOrTouchDownPosition;
}

double WCursor::scrollFactor() const
{
    W_DC(WCursor);
    return d->scrollFactor;
}

void WCursor::setScrollFactor(double factor)
{
    W_D(WCursor);
    if (qFuzzyCompare(d->scrollFactor, factor))
        return;
    d->scrollFactor = factor;
    Q_EMIT scrollFactorChanged();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wcursor.cpp"
