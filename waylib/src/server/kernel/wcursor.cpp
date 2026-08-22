// Copyright (C) 2023 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>
#include "wscoplistener.h"
#include <QCursor>
#include <memory>
#include "private/wprivateaccessor_p.h"

#include "wcursor.h"
#include "private/wcursor_p.h"
#include "winputdevice.h"
#include "wimagebuffer.h"
#include "wseat.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wserver.h"
#include "wayliblogging.h"

#include <wlr_all.h>

#include <QPixmap>
#include <QCoreApplication>
#include <QQuickWindow>
#include <private/qcursor_p.h>

W_DECLARE_PRIVATE_MEMBER(QCursor_d_tag, QCursor, d, QCursorData*);

WAYLIB_SERVER_BEGIN_NAMESPACE

WCursorPrivate::WCursorPrivate(WCursor *qq)
    : WObjectPrivate(qq)
    , overrideCursor(WCursor::toQCursor(WGlobal::CursorShape::Invalid))
{
    m_handle.reset(wlr_cursor_create());
    Q_ASSERT(m_handle);
    m_handle->data = qq;
}

WCursorPrivate::~WCursorPrivate() = default;

WCursor::~WCursor()
{
    teardown();
    W_D(WCursor);
    // Clear the reverse fromHandle() mapping while the native cursor is
    // still alive (the WUniquePointer member releases it after this body).
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    // Owner teardown: detach from the seat and every output before the
    // native cursor is released (WUniquePointer in ~WCursorPrivate, after
    // the listeners are detached there).
    if (d->seat)
        d->seat->setCursor(nullptr);
    if (d->outputLayout) {
        for (auto o : d->outputLayout->outputs())
            o->removeCursor(this);
    }
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
    auto *device = &event->pointer->base;
    const QPointF oldPos = q_func()->position();
    const QPointF delta(event->delta_x, event->delta_y);

    if (!applyPointerConstraint(device, oldPos, delta))
        processCursorMotion(device, event->time_msec);

    // zwp_relative_pointer_v1 always receives the raw hardware delta
    if (Q_LIKELY(seat)) {
        Q_EMIT seat->relativePointerMotion(event->time_msec, delta,
                                           QPointF(event->unaccel_dx, event->unaccel_dy));
    }
}

void WCursorPrivate::on_motion_absolute(wlr_pointer_motion_absolute_event *event)
{
    auto *device = &event->pointer->base;
    const QPointF oldPos = q_func()->position();
    double lx, ly;
    wlr_cursor_absolute_to_layout_coords(handle(), device,
                                         event->x, event->y, &lx, &ly);
    const QPointF rawDelta(lx - oldPos.x(), ly - oldPos.y());

    if (!applyPointerConstraint(device, oldPos, rawDelta))
        processCursorMotion(device, event->time_msec);

    // Same as on_motion: send the raw position-delta as relative motion.
    if (Q_LIKELY(seat)) {
        Q_EMIT seat->relativePointerMotion(event->time_msec, rawDelta, rawDelta);
    }
}

void WCursorPrivate::on_button(wlr_pointer_button_event *event)
{
    auto *device = &event->pointer->base;
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

    if (auto inputDevice = WInputDevice::fromHandle(device)) {
        if (auto deviceSeat = inputDevice->seat()) {
            deviceSeat->notifyButton(q_func(), inputDevice, button, event->state, event->time_msec);
        }
    }
}

void WCursorPrivate::on_axis(wlr_pointer_axis_event *event)
{
    auto *device = &event->pointer->base;

    if (auto inputDevice = WInputDevice::fromHandle(device)) {
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
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(device),
                               event->time_msec, event->fingers, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_update(wlr_pointer_swipe_update_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(device),
                                event->time_msec, delta, 0, 0, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_end(wlr_pointer_swipe_end_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(device),
                             event->time_msec, event->cancelled, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_pinch_begin(wlr_pointer_pinch_begin_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(device),
                              event->time_msec, event->fingers, WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_update(wlr_pointer_pinch_update_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(device),
                                event->time_msec, delta, event->scale, event->rotation,
                                WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_end(wlr_pointer_pinch_end_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(device),
                             event->time_msec, event->cancelled,
                             WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_hold_begin(wlr_pointer_hold_begin_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        seat->notifyHoldBegin(q_func(), WInputDevice::fromHandle(device),
                              event->time_msec, event->fingers);
    }
}

void WCursorPrivate::on_hold_end(wlr_pointer_hold_end_event *event)
{
    auto *device = &event->pointer->base;
    if (Q_LIKELY(seat)) {
        seat->notifyHoldEnd(q_func(), WInputDevice::fromHandle(device),
                            event->time_msec, event->cancelled);
    }
}

void WCursorPrivate::on_touch_down(wlr_touch_down_event *event)
{
    auto *device = &event->touch->base;

    q_func()->setScalePosition(device, QPointF(event->x, event->y));
    lastPressedOrTouchDownPosition = q_func()->position();

    if (Q_LIKELY(seat)) {
        seat->notifyTouchDown(q_func(), WInputDevice::fromHandle(device),
                              event->touch_id, event->time_msec);
    }

}

void WCursorPrivate::on_touch_motion(wlr_touch_motion_event *event)
{
    auto *device = &event->touch->base;

    q_func()->setScalePosition(device, QPointF(event->x, event->y));

    if (Q_LIKELY(seat)) {
        seat->notifyTouchMotion(q_func(), WInputDevice::fromHandle(device),
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
    auto *device = &event->touch->base;

    if (Q_LIKELY(seat)) {
        seat->notifyTouchCancel(q_func(), WInputDevice::fromHandle(device),
                                event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::on_touch_up(wlr_touch_up_event *event)
{
    auto *device = &event->touch->base;

    if (Q_LIKELY(seat)) {
        seat->notifyTouchUp(q_func(), WInputDevice::fromHandle(device),
                            event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::connect()
{
    W_Q(WCursor);
    Q_ASSERT(seat);

    q->listeners()->add(&m_handle->events.motion, this, &WCursorPrivate::on_motion);
    q->listeners()->add(&m_handle->events.motion_absolute, this, &WCursorPrivate::on_motion_absolute);
    q->listeners()->add(&m_handle->events.button, this, &WCursorPrivate::on_button);
    q->listeners()->add(&m_handle->events.axis, this, &WCursorPrivate::on_axis);
    q->listeners()->add(&m_handle->events.frame, this, &WCursorPrivate::on_frame);
    q->listeners()->add(&m_handle->events.swipe_begin, this, &WCursorPrivate::on_swipe_begin);
    q->listeners()->add(&m_handle->events.swipe_update, this, &WCursorPrivate::on_swipe_update);
    q->listeners()->add(&m_handle->events.swipe_end, this, &WCursorPrivate::on_swipe_end);
    q->listeners()->add(&m_handle->events.pinch_begin, this, &WCursorPrivate::on_pinch_begin);
    q->listeners()->add(&m_handle->events.pinch_update, this, &WCursorPrivate::on_pinch_update);
    q->listeners()->add(&m_handle->events.pinch_end, this, &WCursorPrivate::on_pinch_end);
    q->listeners()->add(&m_handle->events.hold_begin, this, &WCursorPrivate::on_hold_begin);
    q->listeners()->add(&m_handle->events.hold_end, this, &WCursorPrivate::on_hold_end);
    // Handle touch device related signals
    q->listeners()->add(&m_handle->events.touch_down, this, &WCursorPrivate::on_touch_down);
    q->listeners()->add(&m_handle->events.touch_motion, this, &WCursorPrivate::on_touch_motion);
    q->listeners()->add(&m_handle->events.touch_frame, this, &WCursorPrivate::on_touch_frame);
    q->listeners()->add(&m_handle->events.touch_cancel, this, &WCursorPrivate::on_touch_cancel);
    q->listeners()->add(&m_handle->events.touch_up, this, &WCursorPrivate::on_touch_up);
}

void WCursorPrivate::processCursorMotion(wlr_input_device *device, uint32_t time)
{
    W_Q(WCursor);

    qCDebug(lcWlPointer) << "Processing cursor motion at" << q->position()
                              << "time:" << time;

    if (auto inputDevice = WInputDevice::fromHandle(device)) {
        if (auto deviceSeat = inputDevice->seat()) {
            deviceSeat->notifyMotion(q, inputDevice, time);
        }
    }
}

bool WCursorPrivate::applyPointerConstraint(wlr_input_device *device,
                                           const QPointF &oldPos,
                                           const QPointF &delta)
{
    W_Q(WCursor);
    if (!activeConstraint) {
        // No constraint: regular delta move.
        q->move(device, delta);
        return false;
    }

    // Touch/tablet input must not be affected by pointer constraints
    if (!device || device->type != WLR_INPUT_DEVICE_POINTER) {
        q->move(device, delta);
        return false;
    }

    wlr_seat *wlrSeat = seat ? seat->handle() : nullptr;
    if (!wlrSeat) {
        q->move(device, delta);
        return false;
    }

    if (activeConstraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        // Warp back to the anchor on every motion so physical movement does
        // not accumulate and cause a jump when the constraint deactivates.
        q->setPosition(device, lockedWarpTarget);
        return true;
    }

    if (activeConstraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED) {
        const double sx = wlrSeat->pointer_state.sx;
        const double sy = wlrSeat->pointer_state.sy;
        double confinedSx = sx + delta.x();
        double confinedSy = sy + delta.y();
        if (wlr_region_confine(&activeConstraint->region, sx, sy,
                               confinedSx, confinedSy,
                               &confinedSx, &confinedSy)) {
            // Confine succeeded: set cursor to the clamped absolute position.
            q->setPosition(device,
                           oldPos + QPointF(confinedSx - sx, confinedSy - sy));
            return false;
        }
        // Cannot enter the constraint region from outside. Block all
        // movement (no cursor move, no dispatch)
        return true;
    }

    // Unknown constraint type — fall through to regular move.
    q->move(device, delta);
    return false;
}

WCursor::WCursor(WCursorPrivate &dd, QObject *parent)
    : QObject(parent)
    , WObject(dd, nullptr)
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

WCursor *WCursor::fromHandle(wlr_cursor *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WCursor*>(handle->data);
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
        removeListeners(this);
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

void WCursor::setActivePointerConstraint(wlr_pointer_constraint_v1 *constraint)
{
    W_D(WCursor);
    if (d->activeConstraint == constraint)
        return;

    d->activeConstraint = constraint;

    if (!constraint)
        return;

    if (constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
        // Anchor the lock to the current cursor position.
        // set_cursor_position_hint is only consumed at unlock time
        // (warpToActiveConstraintHint).
        d->updateLockedWarpTarget();
    }
    // CONFINED: no init-time warp — see canActivate's region-containment check.
    // Activation is deferred until the pointer is inside the constraint region
    // (KWin lazy-activation policy).
}

void WCursorPrivate::updateLockedWarpTarget()
{
    W_Q(WCursor);
    if (!activeConstraint
            || activeConstraint->type != WLR_POINTER_CONSTRAINT_V1_LOCKED)
        return;

    // Per the protocol, set_cursor_position_hint is only consumed at unlock
    // time (warpToActiveConstraintHint). During lock the cursor must stay at
    // a stable anchor so applyPointerConstraint warps back on every motion.
    // Snapping to q->position() is safe because applyPointerConstraint keeps
    // the cursor at lockedWarpTarget between surface commits, so position()
    // is always consistent with the current anchor.
    lockedWarpTarget = q->position();
}

void WCursor::warpToActiveConstraintHint()
{
    W_D(WCursor);
    auto *constraint = d->activeConstraint;
    if (!constraint)
        return;

    wlr_seat *wlrSeat = d->seat ? d->seat->handle() : nullptr;
    // Focus already left this surface (cleared or moved elsewhere) —
    // the surface-local -> global conversion is meaningless.
    if (!wlrSeat || wlrSeat->pointer_state.focused_surface != constraint->surface)
        return;

    if (!constraint->current.cursor_hint.enabled)
        return;

    // Convert the surface-local cursor_hint to global coordinates by
    // computing the offset from the current surface-local pointer position
    // and applying it on top of the current cursor position. This matches
    // the same geometry used in updateLockedWarpTarget() and follows
    // Sway's warp_to_constraint_cursor_hint().
    const double sx = wlrSeat->pointer_state.sx;
    const double sy = wlrSeat->pointer_state.sy;
    const QPointF globalPos = position()
        + QPointF(constraint->current.cursor_hint.x - sx,
                  constraint->current.cursor_hint.y - sy);
    if (!qIsFinite(globalPos.x()) || !qIsFinite(globalPos.y())) {
        qCWarning(lcWlCursor) << "warpToActiveConstraintHint: computed position is"
                             << "not finite (" << globalPos << "), skipping warp";
        return;
    }
    setPosition(nullptr, globalPos);
    wlr_seat_pointer_warp(wlrSeat,
                          constraint->current.cursor_hint.x,
                          constraint->current.cursor_hint.y);
}

wlr_pointer_constraint_v1 *WCursor::activePointerConstraint() const
{
    W_DC(WCursor);
    return d->activeConstraint;
}

QPointF WCursor::position() const
{
    W_DC(WCursor);
    return QPointF(d->handle()->x, d->handle()->y);
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
