// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#define private public
#include <QCursor>
#undef private

#include "wcursor.h"
#include "private/wcursor_p.h"
#include "winputdevice.h"
#include "wimagebuffer.h"
#include "wseat.h"
#include "woutput.h"
#include "woutputlayout.h"

#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwcursor.h>
#include <qwoutput.h>
#include <qwxcursormanager.h>
#include <qwoutputlayout.h>
#include <qwinputdevice.h>
#include <qwpointer.h>
#include <qwtouch.h>
#include <qwseat.h>

#include <QPixmap>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <private/qcursor_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcWlrCursor, "waylib.server.cursor", QtWarningMsg)

WCursorPrivate::WCursorPrivate(WCursor *qq)
    : WWrapObjectPrivate(qq)
{
    initHandle(qw_cursor::create());
    handle()->set_data(this, qq);
}

WCursorPrivate::~WCursorPrivate()
{

}

void WCursorPrivate::instantRelease()
{
    if (seat)
        seat->setCursor(nullptr);

    if (outputLayout) {
        for (auto o : outputLayout->outputs())
            o->removeCursor(q_func());
    }

    delete handle();
}

const QPointingDevice *getDevice(const QString &seatName) {
    const auto devices = QInputDevice::devices();
    for (auto i : devices) {
        if (i->seatName() == seatName && (i->type() == QInputDevice::DeviceType::Mouse
                                          || i->type() == QInputDevice::DeviceType::TouchPad))
            return static_cast<const QPointingDevice*>(i);
    }

    return nullptr;
}

void WCursorPrivate::sendEnterEvent() {
    if (enterWindowEventHasSend)
        return;

    W_Q(WCursor);

    auto device = getDevice(seat->name());
    Q_ASSERT(device && WInputDevice::from(device));

    if (WInputDevice::from(device)->seat()) {
        enterWindowEventHasSend = true;
        const QPointF global = q->position();
        const QPointF local = global - eventWindow->position();
        QEnterEvent event(local, local, global, device);
        QCoreApplication::sendEvent(eventWindow, &event);
    }
}

void WCursorPrivate::sendLeaveEvent()
{
    if (leaveWindowEventHasSend)
        return;

    auto device = getDevice(seat->name());
    if (!device)
        return;
    if (!WInputDevice::from(device))
        return;
    if (WInputDevice::from(device)->seat()) {
        leaveWindowEventHasSend = true;
        QInputEvent event(QEvent::Leave, device);
        QCoreApplication::sendEvent(eventWindow, &event);
    }
}

void WCursorPrivate::on_motion(wlr_pointer_motion_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    q_func()->move(device, QPointF(event->delta_x, event->delta_y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_motion_absolute(wlr_pointer_motion_absolute_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    q_func()->setScalePosition(device, QPointF(event->x, event->y));
    processCursorMotion(device, event->time_msec);
}

void WCursorPrivate::on_button(wlr_pointer_button_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    button = WCursor::fromNativeButton(event->button);

    if (event->state == WLR_BUTTON_RELEASED) {
        state &= ~button;
    } else {
        state |= button;
        lastPressedOrTouchDownPosition = q_func()->position();
    }

    if (Q_LIKELY(seat)) {
        seat->notifyButton(q_func(), WInputDevice::fromHandle(device),
                           button, event->state, event->time_msec);
    }
}

void WCursorPrivate::on_axis(wlr_pointer_axis_event *event)
{
    auto device = qw_pointer::from(event->pointer);

    if (Q_LIKELY(seat)) {
        seat->notifyAxis(q_func(), WInputDevice::fromHandle(device), event->source,
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

void WCursorPrivate::on_swipe_begin(wlr_pointer_swipe_begin_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(device),
                               event->time_msec, event->fingers, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_update(wlr_pointer_swipe_update_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(device),
                                event->time_msec, delta, 0, 0, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_swipe_end(wlr_pointer_swipe_end_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(device),
                             event->time_msec, event->cancelled, WGestureEvent::SwipeGesture);
    }
}

void WCursorPrivate::on_pinch_begin(wlr_pointer_pinch_begin_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureBegin(q_func(), WInputDevice::fromHandle(device),
                              event->time_msec, event->fingers, WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_update(wlr_pointer_pinch_update_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        QPointF delta = QPointF(event->dx, event->dy);
        seat->notifyGestureUpdate(q_func(), WInputDevice::fromHandle(device),
                                event->time_msec, delta, event->scale, event->rotation,
                                WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_pinch_end(wlr_pointer_pinch_end_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyGestureEnd(q_func(), WInputDevice::fromHandle(device),
                             event->time_msec, event->cancelled,
                             WGestureEvent::PinchGesture);
    }
}

void WCursorPrivate::on_hold_begin(wlr_pointer_hold_begin_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyHoldBegin(q_func(), WInputDevice::fromHandle(device),
                              event->time_msec, event->fingers);
    }
}

void WCursorPrivate::on_hold_end(wlr_pointer_hold_end_event *event)
{
    auto device = qw_pointer::from(event->pointer);
    if (Q_LIKELY(seat)) {
        seat->notifyHoldEnd(q_func(), WInputDevice::fromHandle(device),
                            event->time_msec, event->cancelled);
    }
}

void WCursorPrivate::on_touch_down(wlr_touch_down_event *event)
{
    auto device = qw_touch::from(event->touch);

    q_func()->setScalePosition(device, QPointF(event->x, event->y));
    lastPressedOrTouchDownPosition = q_func()->position();

    if (Q_LIKELY(seat)) {
        seat->notifyTouchDown(q_func(), WInputDevice::fromHandle(device),
                              event->touch_id, event->time_msec);
    }

}

void WCursorPrivate::on_touch_motion(wlr_touch_motion_event *event)
{
    auto device = qw_touch::from(event->touch);

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
    auto device = qw_touch::from(event->touch);

    if (Q_LIKELY(seat)) {
        seat->notifyTouchCancel(q_func(), WInputDevice::fromHandle(device),
                                event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::on_touch_up(wlr_touch_up_event *event)
{
    auto device = qw_touch::from(event->touch);

    if (Q_LIKELY(seat)) {
        seat->notifyTouchUp(q_func(), WInputDevice::fromHandle(device),
                            event->touch_id, event->time_msec);
    }
}

void WCursorPrivate::connect()
{
    W_Q(WCursor);
    Q_ASSERT(seat);

    QObject::connect(handle(), &qw_cursor::notify_motion, q, [this] (wlr_pointer_motion_event *event) {
        on_motion(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_motion_absolute, q, [this] (wlr_pointer_motion_absolute_event *event) {
        on_motion_absolute(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_button, q, [this] (wlr_pointer_button_event *event) {
        on_button(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_axis, q, [this] (wlr_pointer_axis_event *event) {
        on_axis(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_frame, q, [this] () {
        on_frame();
    });

    QObject::connect(handle(), SIGNAL(notify_swipe_begin(wlr_pointer_swipe_begin_event*)),
                     q, SLOT(on_swipe_begin(wlr_pointer_swipe_begin_event*)));
    QObject::connect(handle(), SIGNAL(notify_swipe_update(wlr_pointer_swipe_update_event*)),
                     q, SLOT(on_swipe_update(wlr_pointer_swipe_update_event*)));
    QObject::connect(handle(), SIGNAL(notify_swipe_end(wlr_pointer_swipe_end_event*)),
                     q, SLOT(on_swipe_end(wlr_pointer_swipe_end_event*)));
    QObject::connect(handle(), SIGNAL(notify_pinch_begin(wlr_pointer_pinch_begin_event*)),
                     q, SLOT(on_pinch_begin(wlr_pointer_pinch_begin_event*)));
    QObject::connect(handle(), SIGNAL(notify_pinch_update(wlr_pointer_pinch_update_event*)),
                     q, SLOT(on_pinch_update(wlr_pointer_pinch_update_event*)));
    QObject::connect(handle(), SIGNAL(notify_pinch_end(wlr_pointer_pinch_end_event*)),
                     q, SLOT(on_pinch_end(wlr_pointer_pinch_end_event*)));
    QObject::connect(handle(), SIGNAL(notify_hold_begin(wlr_pointer_hold_begin_event*)),
                     q, SLOT(on_hold_begin(wlr_pointer_hold_begin_event*)));
    QObject::connect(handle(), SIGNAL(notify_hold_end(wlr_pointer_hold_end_event*)),
                     q, SLOT(on_hold_end(wlr_pointer_hold_end_event*)));

    // Handle touch device related signals
    QObject::connect(handle(), &qw_cursor::notify_touch_down, q, [this] (wlr_touch_down_event *event) {
        on_touch_down(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_touch_motion, q, [this] (wlr_touch_motion_event *event) {
        on_touch_motion(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_touch_frame, q, [this] () {
        on_touch_frame();
    });
    QObject::connect(handle(), &qw_cursor::notify_touch_cancel, q, [this] (wlr_touch_cancel_event *event) {
        on_touch_cancel(event);
    });
    QObject::connect(handle(), &qw_cursor::notify_touch_up, q, [this] (wlr_touch_up_event *event) {
        on_touch_up(event);
    });
}

void WCursorPrivate::processCursorMotion(qw_pointer *device, uint32_t time)
{
    W_Q(WCursor);

    if (Q_LIKELY(seat))
        seat->notifyMotion(q, WInputDevice::fromHandle(device), time);
}

WCursor::WCursor(WCursorPrivate &dd, QObject *parent)
    : WWrapObject(dd, parent)
{

}

void WCursor::move(qw_input_device *device, const QPointF &delta)
{
    const QPointF oldPos = position();
    d_func()->handle()->move(*device, delta.x(), delta.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
}

void WCursor::setPosition(qw_input_device *device, const QPointF &pos)
{
    const QPointF oldPos = position();
    d_func()->handle()->warp_closest(*device, pos.x(), pos.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
}

bool WCursor::setPositionWithChecker(qw_input_device *device, const QPointF &pos)
{
    const QPointF oldPos = position();
    bool ok = d_func()->handle()->warp(*device, pos.x(), pos.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
    return ok;
}

void WCursor::setScalePosition(qw_input_device *device, const QPointF &ratio)
{
    const QPointF oldPos = position();
    d_func()->handle()->warp_absolute(*device, ratio.x(), ratio.y());

    if (oldPos != position())
        Q_EMIT positionChanged();
}

WCursor::WCursor(QObject *parent)
    : WCursor(*new WCursorPrivate(this), parent)
{

}

qw_cursor *WCursor::handle() const
{
    W_DC(WCursor);
    return d->handle();
}

WCursor *WCursor::fromHandle(const qw_cursor *handle)
{
    return handle->get_data<WCursor>();
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
    default: qWarning() << "invalid Qt's button" << button;
    }

    return 0;
}

QCursor WCursor::toQCursor(CursorShape shape)
{
    static QBitmap tmp(1, 1);
    // Ensure alloc a new QCursorData
    QCursor cursor(tmp, tmp);

    Q_ASSERT(cursor.d->ref == 1);
    Q_ASSERT(cursor.d->bm);
    Q_ASSERT(cursor.d->bmm);
    delete cursor.d->bm;
    delete cursor.d->bmm;
    cursor.d->bm = nullptr;
    cursor.d->bmm = nullptr;
    cursor.d->cshape = static_cast<Qt::CursorShape>(shape);

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
        d->handle()->disconnect(this);
        d->seat->disconnect(this);
    }
    d->seat = seat;

    if (d->seat) {
        d->connect();

        connect(d->seat, &WSeat::requestCursorShape, this, &WCursor::requestedCursorShapeChanged);
        connect(d->seat, &WSeat::requestCursorSurface, this, &WCursor::requestedCursorSurfaceChanged);
        connect(d->seat, &WSeat::requestDrag, this, &WCursor::requestedDragSurfaceChanged);

        if (d->eventWindow) {
            d->sendEnterEvent();
        }
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
        d->sendLeaveEvent();
    }

    d->eventWindow = window;
    d->enterWindowEventHasSend = false;
    d->leaveWindowEventHasSend = false;

    if (d->eventWindow && d->seat && !d->deviceList.isEmpty()) {
        d->sendEnterEvent();
    }
}

Qt::CursorShape WCursor::defaultCursor()
{
    return Qt::ArrowCursor;
}

QCursor WCursor::cursor() const
{
    W_DC(WCursor);
    return d->cursor;
}

void WCursor::setCursor(const QCursor &cursor)
{
    W_D(WCursor);

    if (d->cursor == cursor)
        return;
    d->cursor = cursor;
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
        return false;
    }

    W_D(WCursor);
    Q_ASSERT(!d->deviceList.contains(device));
    d->handle()->attach_input_device(device->handle()->handle());
    d->deviceList << device;

    if (d->eventWindow && d->deviceList.count() == 1) {
        Q_ASSERT(d->seat);
        d->sendEnterEvent();
    }

    return true;
}

void WCursor::detachInputDevice(WInputDevice *device)
{
    W_D(WCursor);

    if (!d->deviceList.removeOne(device))
        return;

    d->handle()->detach_input_device(device->handle()->handle());
    d->handle()->map_input_to_output(device->handle()->handle(), nullptr);

    if (d->eventWindow && d->deviceList.isEmpty()) {
        Q_ASSERT(d->seat);
        d->sendLeaveEvent();
    }
}

void WCursor::setLayout(WOutputLayout *layout)
{
    W_D(WCursor);

    if (d->outputLayout == layout)
        return;

    d->outputLayout = layout;
    d->handle()->attach_output_layout(*d->outputLayout);

    if (d->outputLayout) {
        for (auto o : d->outputLayout->outputs())
            o->addCursor(this);
    }

    connect(d->outputLayout, &WOutputLayout::outputAdded, this, [this, d] (WOutput *o) {
        o->addCursor(this);
    });

    connect(d->outputLayout, &WOutputLayout::outputRemoved, this, [this, d] (WOutput *o) {
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

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wcursor.cpp"
