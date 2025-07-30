// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wseat.h"
#include "wcursor.h"
#include "winputdevice.h"
#include "woutput.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "platformplugin/qwlrootsintegration.h"
#include "private/wglobal_p.h"

#include <qwseat.h>
#include <qwkeyboard.h>
#include <qwcursor.h>
#include <qwcompositor.h>
#include <qwdatadevice.h>
#include <qwpointergesturesv1.h>
#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwprimaryselection.h>

#include <QQuickWindow>
#include <QGuiApplication>
#include <QQuickItem>
#include <QDebug>
#include <QTimer>

#include <qpa/qwindowsysteminterface.h>
#include <private/qxkbcommon_p.h>
#include <private/qquickwindow_p.h>
#include <private/qquickdeliveryagent_p_p.h>

QT_BEGIN_NAMESPACE
Q_GUI_EXPORT bool qt_sendShortcutOverrideEvent(QObject *o, ulong timestamp, int k, Qt::KeyboardModifiers mods, const QString &text = QString(), bool autorep = false, ushort count = 1);
QT_END_NAMESPACE

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcWlrTouch, "waylib.server.seat", QtWarningMsg)
Q_LOGGING_CATEGORY(qLcWlrTouchEvents, "waylib.server.seat.events.touch", QtWarningMsg)
Q_LOGGING_CATEGORY(qLcWlrDragEvents, "waylib.server.seat.events.drag", QtWarningMsg)
Q_LOGGING_CATEGORY(qLcWlrGestureEvents, "waylib.server.seat.events.gesture", QtWarningMsg)

#if QT_CONFIG(wheelevent)
class Q_DECL_HIDDEN WSeatWheelEvent : public QWheelEvent {
public:
    WSeatWheelEvent(wl_pointer_axis_source wlr_source, double wlr_delta, Qt::Orientation orientation,
                    wl_pointer_axis_relative_direction rd,
                    const QPointF &pos, const QPointF &globalPos, QPoint pixelDelta, QPoint angleDelta,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, Qt::ScrollPhase phase,
                    bool inverted, Qt::MouseEventSource source = Qt::MouseEventNotSynthesized,
                    const QPointingDevice *device = QPointingDevice::primaryPointingDevice())
        : QWheelEvent(pos, globalPos, pixelDelta, angleDelta, buttons, modifiers, phase, inverted, source, device)
        , m_wlrSource(wlr_source)
        , m_wlrDelta(wlr_delta)
        , m_orientation(orientation)
        , m_relativeDirection(rd)
    {

    }

    inline wl_pointer_axis_source wlrSource() const { return m_wlrSource; }
    inline double wlrDelta() const { return m_wlrDelta; }
    inline Qt::Orientation orientation() const { return m_orientation; }
    inline wl_pointer_axis_relative_direction relativeDirection() const { return m_relativeDirection; }

protected:
    wl_pointer_axis_source m_wlrSource;
    double m_wlrDelta;
    Qt::Orientation m_orientation;
    wl_pointer_axis_relative_direction m_relativeDirection;
};
#endif

class Q_DECL_HIDDEN WSeatPrivate : public WWrapObjectPrivate
{
public:
    WSeatPrivate(WSeat *qq, const QString &name)
        : WWrapObjectPrivate(qq)
        , name(name)
    {
        pendingEvents.reserve(2);

        m_repeatTimer.callOnTimeout([&](){
            if (!focusWindow) {
                return;
            }
            auto rawdevice = qobject_cast<qw_keyboard*>(WInputDevice::from(m_repeatKey->device())->handle())->handle();
            m_repeatTimer.setInterval(1000 / rawdevice->repeat_info.rate);
            auto evPress = QKeyEvent(QEvent::KeyPress, m_repeatKey->key(), m_repeatKey->modifiers(),
                m_repeatKey->nativeScanCode(), m_repeatKey->nativeVirtualKey(), m_repeatKey->nativeModifiers(),
                m_repeatKey->text(), true, m_repeatKey->count(), m_repeatKey->device());
            auto evRelease = QKeyEvent(QEvent::KeyRelease, m_repeatKey->key(), m_repeatKey->modifiers(),
                m_repeatKey->nativeScanCode(), m_repeatKey->nativeVirtualKey(), m_repeatKey->nativeModifiers(),
                m_repeatKey->text(), true, m_repeatKey->count(), m_repeatKey->device());
            evPress.setTimestamp(m_repeatKey->timestamp());
            evRelease.setTimestamp(m_repeatKey->timestamp());
            handleKeyEvent(evPress);
            handleKeyEvent(evRelease);
        });
    }
    ~WSeatPrivate() {
        if (onEventObjectDestroy)
            QObject::disconnect(onEventObjectDestroy);

        for (auto device : std::as_const(deviceList))
            detachInputDevice(device);
    }

    inline qw_seat *handle() const {
        return q_func()->nativeInterface<qw_seat>();
    }

    inline wlr_seat *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    inline wlr_surface *pointerFocusSurface() const {
        return nativeHandle()->pointer_state.focused_surface;
    }

    inline wlr_surface *keyboardFocusSurface() const {
        return nativeHandle()->keyboard_state.focused_surface;
    }

    inline bool doNotifyMotion(WSurface *target, QObject *eventObject, QPointF localPos, uint32_t timestamp) {
        if (target) {
            if (pointerFocusSurface()) {
                Q_ASSERT(pointerFocusEventObject == eventObject);
                Q_ASSERT(pointerFocusSurface() == target->handle()->handle());
            } else {
                // Maybe this seat is grabbed by a xdg popup surface, so the surface of under mouse
                // can't take pointer focus, but maybe the popup is closed now, so we should try again
                // take pointer focus for this surface.
                doEnter(target, eventObject, localPos);
            }
        }

        handle()->pointer_notify_motion(timestamp, localPos.x(), localPos.y());
        return true;
    }
    inline bool doNotifyButton(uint32_t button, wl_pointer_button_state state, uint32_t timestamp) {
        handle()->pointer_notify_button(timestamp, button, state);
        return true;
    }
    static inline wl_pointer_axis fromQtHorizontal(Qt::Orientation o) {
        return o == Qt::Horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL
                                   : WL_POINTER_AXIS_VERTICAL_SCROLL;
    }
    inline bool doNotifyAxis(wl_pointer_axis_source source, Qt::Orientation orientation,
                             wl_pointer_axis_relative_direction relative_direction,
                             double delta, int32_t delta_discrete, uint32_t timestamp) {
        if (!pointerFocusSurface())
            return false;

        handle()->pointer_notify_axis(timestamp, fromQtHorizontal(orientation), delta, delta_discrete,
                                      source, relative_direction);
        return true;
    }
    inline void doNotifyFrame() {
        handle()->pointer_notify_frame();
    }
    inline bool doEnter(WSurface *surface, QObject *eventObject, const QPointF &position) {
        // doEnter be called from QEvent::HoverEnter is normal,
        // but doNotifyMotion will call doEnter too,
        // so should compare pointerFocusEventObject and eventObject early
        if (pointerFocusEventObject == eventObject) {
            return true;
        }
        auto tmp = oldPointerFocusSurface;
        oldPointerFocusSurface = handle()->handle()->pointer_state.focused_surface;
        handle()->pointer_notify_enter(surface->handle()->handle(), position.x(), position.y());
        if (!pointerFocusSurface()) {
            // Because if the last pointer focus surface is a popup, the 'pointerNotifyEnter'
            // will call 'xdg_pointer_grab_enter' in wlroots, and the 'xdg_pointer_grab_enter'
            // will call 'wlr_seat_pointer_clear_focus' if the surface's client and the popup's
            // client is not equal.
            oldPointerFocusSurface = tmp;
            return false;
        }
        Q_ASSERT(pointerFocusSurface() == surface->handle()->handle());

        Q_ASSERT(!pointerFocusEventObject || eventObject != pointerFocusEventObject);
        if (pointerFocusEventObject) {
            Q_ASSERT(onEventObjectDestroy);
            QObject::disconnect(onEventObjectDestroy);
        }
        pointerFocusEventObject = eventObject;
        if (eventObject) {
            onEventObjectDestroy = QObject::connect(eventObject, &QObject::destroyed,
                                                    q_func(), [this] {
                doClearPointerFocus();
            });
        }

        return true;
    }
    inline void doClearPointerFocus() {
        pointerFocusEventObject.clear();
        handle()->pointer_notify_clear_focus();
        Q_ASSERT(!handle()->handle()->pointer_state.focused_surface);
        if (cursor) // reset cursur from QCursor resource, the last cursor is from wlr_surface
            cursor->setCursor(cursor->cursor());
    }
    inline void doSetKeyboardFocus(qw_surface *surface) {
        if (surface) {
            handle()->keyboard_enter(*surface, nullptr, 0, nullptr);
        } else {
            handle()->keyboard_clear_focus();
        }
    }
    inline void doTouchNotifyDown(WSurface *surface, uint32_t time_msec, int32_t touch_id, const QPointF &pos) {
        handle()->touch_notify_down(surface->handle()->handle(), time_msec, touch_id, pos.x(), pos.y());
    }
    inline void doTouchNotifyMotion(uint32_t time_msec, int32_t touch_id, const QPointF &pos) {
        handle()->touch_notify_motion(time_msec, touch_id, pos.x(), pos.y());
    }
    inline void doTouchNotifyUp(uint32_t time_msec, int32_t touch_id) {
        handle()->touch_notify_up(time_msec, touch_id);
    }
    inline void doTouchNotifyCancel(WInputDevice *device) {
        auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();
        for (int i = 0; i < state->m_points.size(); ++i) {
            const auto &qtPoint = state->m_points.at(i);
            if (qtPoint.state == static_cast<QEventPoint::State>(WEvent::PointCancelled)) {
                auto point = handle()->touch_get_point(qtPoint.id);
                Q_ASSERT(point);
                state->m_points.removeAt(i--);
                handle()->touch_notify_cancel(point->client);
            }
        }
    }
    inline void doNotifyFullTouchEvent(WSurface *surface, int32_t touch_id, const QPointF &position, QEventPoint::State state, uint32_t time_msec) {
        switch (state) {
        using enum QEventPoint::State;
        case Pressed:
            doTouchNotifyDown(surface, time_msec, touch_id, position);
            break;
        case Updated:
            doTouchNotifyMotion(time_msec, touch_id, position);
            break;
        case Released:
            doTouchNotifyUp(time_msec, touch_id);
            break;
        case Stationary:
            // stationary points are not sent through wayland, the client must cache them
            break;
        case Unknown:
            // Ignored
            break;
        }
    }

    inline void doNotifyTouchFrame(WInputDevice *device) {
        auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
        Q_ASSERT(qwDevice);
        auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();

        qCDebug(qLcWlrTouchEvents) << "Touch frame for device: " << qwDevice->name()
                                   << ", handle the following state: " << state->m_points;

        if (state->m_points.isEmpty())
            return;

        if (cursor->eventWindow()) {
            QWindowSystemInterface::handleTouchEvent(cursor->eventWindow(), qwDevice, state->m_points,
                                                     keyModifiers);
        }

        for (int i = 0; i < state->m_points.size(); ++i) {
            QWindowSystemInterface::TouchPoint &tp(state->m_points[i]);
            if (tp.state == QEventPoint::Released)
                state->m_points.removeAt(i--);
            else if (tp.state == QEventPoint::Pressed)
                tp.state = QEventPoint::Stationary;
            else if (tp.state == QEventPoint::Updated)
                tp.state = QEventPoint::Stationary;  // notiyfy: qtbase don't change Updated
            else if (tp.state != QEventPoint::Stationary)
                Q_UNREACHABLE_RETURN();
        }
        handle()->touch_notify_frame();
    }

    // for keyboard event
    inline bool doNotifyKey(WInputDevice *device, uint32_t keycode, uint32_t state, uint32_t timestamp) {
        if (!keyboardFocusSurface())
            return false;

        q_func()->setKeyboard(device);
        /* Send modifiers to the client. */
        this->handle()->keyboard_notify_key(timestamp, keycode, state);
        return true;
    }
    inline bool doNotifyModifiers(WInputDevice *device) {
        if (!keyboardFocusSurface())
            return false;

        auto keyboard = qobject_cast<qw_keyboard*>(device->handle());
        q_func()->setKeyboard(device);
        /* Send modifiers to the client. */
        this->handle()->keyboard_notify_modifiers(&keyboard->handle()->modifiers);
        return true;
    }
    inline void doMouseMove(WCursor *cursor, const QPointingDevice *device, uint32_t timestamp) {
        Q_ASSERT(device);
        QWindow *w = cursor->eventWindow();
        const QPointF &global = cursor->position();
        const QPointF local = w ? global - QPointF(w->position()) : QPointF();

        QMouseEvent e(QEvent::MouseMove, local, global, Qt::NoButton,
                      cursor->state(), keyModifiers, device);
        Q_ASSERT(e.isUpdateEvent());
        e.setTimestamp(timestamp);

        if (w)
            QCoreApplication::sendEvent(w, &e);
    }

    // begin slot function
    void on_destroy();
    void on_request_set_cursor(wlr_seat_pointer_request_set_cursor_event *event);
    void on_request_set_selection(wlr_seat_request_set_selection_event *event);
    void on_request_set_primary_selection(wlr_seat_request_set_primary_selection_event *event);
    void on_request_start_drag(wlr_seat_request_start_drag_event *event);
    void on_start_drag(wlr_drag *drag);

    void on_keyboard_key(wlr_keyboard_key_event *event, WInputDevice *device);
    void on_keyboard_modifiers(WInputDevice *device);
    // end slot function

    void connect();
    void updateCapabilities();
    void attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);
    // handle spontaneous & synthetic key event for focusWindow
    void handleKeyEvent(QKeyEvent &e);

    W_DECLARE_PUBLIC(WSeat)

    QString name;
    WCursor *cursor = nullptr;
    qw_pointer_gestures_v1 *gesture = nullptr;
    QVector<WInputDevice*> deviceList;
    QVector<WInputDevice*> touchDeviceList;
    QPointer<WSeatEventFilter> eventFilter;
    QPointer<QWindow> focusWindow;
    QPointer<QObject> pointerFocusEventObject;
    QPointer<WSurface> m_keyboardFocusSurface;
    QMetaObject::Connection onEventObjectDestroy;
    wlr_surface *oldPointerFocusSurface = nullptr;

    bool gestureActive = false;
    int gestureFingers = 0;
    qreal lastScale = 1.0;

    struct EventState {
        // Don't use it, its may be a invalid pointer
        void *event;
        quint64 timestamp;
        // ###: It's only using compare pointer value.
        // It's for a Qt bug. When handling mouse events in QQuickDeliveryAgentPrivate::deliverPressOrReleaseEvent,
        // if there are multiple QQuickItems that can receive the mouse events where the mouse is pressed, Qt will
        // attempt to dispatch them one by one. Even if the top-level QQuickItem has already accepted the event,
        // QQuickDeliveryAgentPrivate will still call setAccepted(false) to set the acceptance status to false for
        // each mouse point in the QPointerEvent. Then it will try to pass the event to the QQuickPointerHandler
        // objects of the underlying QQuickItems for processing. Although no QQuickPointerHandler receives the event,
        // the above behavior has already caused QPointerEvent::allPointsAccepted to return false. This will cause
        // QQuickDeliveryAgentPrivate::deliverPressOrReleaseEvent to return false, ultimately causing
        // QQuickDeliveryAgentPrivate::deliverPointerEvent to believe that the event has not been accepted and set the
        // accepted status of QEvent to false. This leads to WSeat considering the event unused, and then it is passed
        // to WSeatEventFilter::unacceptedEvent.
        bool isAccepted;
    };
    QList<EventState> pendingEvents;

    inline EventState *addEventState(QInputEvent *event) {
        Q_ASSERT(indexOfEventState(event) < 0);
        pendingEvents.append({.event = event, .timestamp = event->timestamp(), .isAccepted = true});
        return &pendingEvents.last();
    }
    inline int indexOfEventState(QInputEvent *event) const {
        for (int i = 0; i < pendingEvents.size(); ++i)
            if (pendingEvents.at(i).event == event
                    && pendingEvents.at(i).timestamp == event->timestamp())
                return i;
        return -1;
    }
    inline EventState *getEventState(QInputEvent *event) {
        int index = indexOfEventState(event);
        return index < 0 ? nullptr : &pendingEvents[index];
    }

    // for event data
    Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;

    // for touch event
    struct DeviceState {
        DeviceState() { }
        QList<QWindowSystemInterface::TouchPoint> m_points;
        inline QWindowSystemInterface::TouchPoint *point(int32_t touch_id) {
            for (int i = 0; i < m_points.size(); ++i)
                if (m_points.at(i).id == touch_id)
                    return &m_points[i];
            return nullptr;
        }
    };

    // for keyboard event
    QTimer m_repeatTimer;
    std::unique_ptr<QKeyEvent> m_repeatKey;

    // for cursor data
    // TODO: make to QWSeatClient in wlroots
    // Don't access its member, maybe is a invalid pointer
    wlr_seat_client *cursorClient = nullptr;
    QPointer<WSurface> cursorSurface;
    QPoint cursorSurfaceHotspot;
    WGlobal::CursorShape cursorShape = WGlobal::CursorShape::Invalid;

    QPointer<WSurface> dragSurface;

    bool alwaysUpdateHoverTarget = false;
};

void WSeatPrivate::on_destroy()
{
    q_func()->m_handle = nullptr;
}

void WSeatPrivate::on_request_set_cursor(wlr_seat_pointer_request_set_cursor_event *event)
{
    auto focused_client = nativeHandle()->pointer_state.focused_client;
    /* This can be sent by any client, so we check to make sure this one is
     * actually has pointer focus first. */
    if (focused_client == event->seat_client) {
        /* Once we've vetted the client, we can tell the cursor to use the
         * provided surface as the cursor image. It will set the hardware cursor
         * on the output that it's currently on and continue to do so as the
         * cursor moves between outputs. */
        auto *surface = event->surface ? qw_surface::from(event->surface) : nullptr;
        cursorClient = event->seat_client;
        cursorShape = WGlobal::CursorShape::Invalid;

        if (cursorSurface)
            cursorSurface->safeDeleteLater();

        W_Q(WSeat);
        if (surface) {
            cursorSurface = new WSurface(surface, q);
            QObject::connect(surface, &qw_surface::before_destroy,
                             cursorSurface, &WSurface::safeDeleteLater);
        } else {
            cursorSurface.clear();
        }
        cursorSurfaceHotspot.rx() = event->hotspot_x;
        cursorSurfaceHotspot.ry() = event->hotspot_y;

        Q_EMIT q->requestCursorSurface(cursorSurface, cursorSurfaceHotspot);
    }
}

void WSeatPrivate::on_request_set_selection(wlr_seat_request_set_selection_event *event)
{
    handle()->set_selection(event->source, event->serial);
}

void WSeatPrivate::on_request_set_primary_selection(wlr_seat_request_set_primary_selection_event *event)
{
    wlr_seat_set_primary_selection(nativeHandle(), event->source, event->serial);
}

void WSeatPrivate::on_request_start_drag(wlr_seat_request_start_drag_event *event)
{
    if (handle()->validate_pointer_grab_serial(event->origin, event->serial)) {
        wlr_seat_start_pointer_drag(nativeHandle(), event->drag, event->serial);
        return;
    }

    struct wlr_touch_point *point;
    if (handle()->validate_touch_grab_serial(event->origin, event->serial, &point)) {
        wlr_seat_start_touch_drag(nativeHandle(), event->drag, event->serial, point);
        return;
    }

    qCWarning(qLcWlrDragEvents) << "Ignoring start_drag request: "
                                << "could not validate pointer or touch serial " << event->serial;

    wlr_data_source_destroy(event->drag->source);
}

void WSeatPrivate::on_start_drag(wlr_drag *drag)
{
    doClearPointerFocus();
    if (drag->icon) {
        W_Q(WSeat);
        auto *surface = qw_surface::from(drag->icon->surface);
        auto *wsurface = new WSurface(surface, q);
        QObject::connect(surface, &qw_surface::before_destroy,
                         wsurface, &WSurface::safeDeleteLater);
        if (dragSurface)
            dragSurface->safeDeleteLater();
        dragSurface = wsurface;
        Q_EMIT q->requestDrag(dragSurface.get());
    }
}
void WSeatPrivate::handleKeyEvent(QKeyEvent &e)
{
    Q_ASSERT(focusWindow);
    if (e.type() == QEvent::KeyPress && QWindowSystemInterface::handleShortcutEvent(focusWindow,
                                     e.timestamp(), e.key(), e.modifiers(), e.nativeScanCode(),
                                     e.nativeVirtualKey(), e.nativeModifiers(),
                                     e.text(), e.isAutoRepeat(), e.count())) {
        return;
    }
    QCoreApplication::sendEvent(focusWindow, &e);
}
void WSeatPrivate::on_keyboard_key(wlr_keyboard_key_event *event, WInputDevice *device)
{
    auto keyboard = qobject_cast<qw_keyboard*>(device->handle());

    auto code = event->keycode + 8; // map to wl_keyboard::keymap_format::keymap_format_xkb_v1
    auto et = event->state == WL_KEYBOARD_KEY_STATE_PRESSED ? QEvent::KeyPress : QEvent::KeyRelease;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(keyboard->handle()->xkb_state, code);

    // Qt doesn't support XF86Switch_VT_1 to XF86Switch_VT_12, so convert them to
    // Ctrl+Alt+F1 to Ctrl+Alt+F12
    //
    // Assumption: XKB_KEY_XF86Switch_VT_1 and XKB_KEY_F1 are contiguous and ordered such that
    // (XKB_KEY_F1 + (sym - XKB_KEY_XF86Switch_VT_1)) yields the correct F-key.
    // If this is not true, the calculation below may be unsafe.
    static_assert(
        (XKB_KEY_XF86Switch_VT_12 - XKB_KEY_XF86Switch_VT_1) == (XKB_KEY_F12 - XKB_KEY_F1),
        "XKB_KEY_XF86Switch_VT_1..12 and XKB_KEY_F1..F12 must be contiguous and ordered for keysym calculation"
    );
    if (sym >= XKB_KEY_XF86Switch_VT_1 && sym <= XKB_KEY_XF86Switch_VT_12) {
        if (keyModifiers == (Qt::ControlModifier | Qt::AltModifier)) {
            sym = XKB_KEY_F1 + (sym - XKB_KEY_XF86Switch_VT_1);
        }
    }

    int qtkey = QXkbCommon::keysymToQtKey(sym, keyModifiers, keyboard->handle()->xkb_state, code);
    const QString &text = QXkbCommon::lookupString(keyboard->handle()->xkb_state, code);

    QKeyEvent e(et, qtkey, keyModifiers, code, event->keycode, keyboard->get_modifiers(),
                text, false, 1, device->qtDevice());
    e.setTimestamp(event->time_msec);

    if (focusWindow) {
        handleKeyEvent(e);
        if (et == QEvent::KeyPress && xkb_keymap_key_repeats(keyboard->handle()->keymap, code)) {
            if (m_repeatKey) {
                m_repeatTimer.stop();
            }
            m_repeatKey = std::make_unique<QKeyEvent>(et, qtkey, keyModifiers, code, event->keycode, keyboard->get_modifiers(),
                text, false, 1, device->qtDevice());
            m_repeatKey->setTimestamp(event->time_msec);
            m_repeatTimer.setInterval(keyboard->handle()->repeat_info.delay);
            m_repeatTimer.start();
        } else if (et == QEvent::KeyRelease && m_repeatKey && m_repeatKey->nativeScanCode() == code) {
            m_repeatTimer.stop();
            m_repeatKey.reset();
        }
    } else {
        if (et == QEvent::KeyPress && qt_sendShortcutOverrideEvent((QObject*)qGuiApp,
                                        e.timestamp(), e.key(), e.modifiers(),
                                        e.text(), e.isAutoRepeat(), e.count())) {
            return;
        }
        doNotifyKey(device, event->keycode, event->state, event->time_msec);
    }
}

void WSeatPrivate::on_keyboard_modifiers(WInputDevice *device)
{
    auto keyboard = qobject_cast<qw_keyboard*>(device->handle());
    keyModifiers = QXkbCommon::modifiers(keyboard->handle()->xkb_state);
    doNotifyModifiers(device);
}

void WSeatPrivate::connect()
{
    W_Q(WSeat);
    QObject::connect(handle(), &qw_seat::destroyed, q, [this] {
        on_destroy();
    });
    QObject::connect(handle(), &qw_seat::notify_request_set_cursor, q, [this] (wlr_seat_pointer_request_set_cursor_event *event) {
        on_request_set_cursor(event);
    });
    QObject::connect(handle(), &qw_seat::notify_request_set_selection, q, [this] (wlr_seat_request_set_selection_event *event) {
        on_request_set_selection(event);
    });
    QObject::connect(handle(), &qw_seat::notify_request_set_primary_selection, q, [this] (wlr_seat_request_set_primary_selection_event *event) {
        on_request_set_primary_selection(event);
    });
    QObject::connect(handle(), &qw_seat::notify_request_start_drag, q, [this] (wlr_seat_request_start_drag_event *event) {
        on_request_start_drag(event);
    });
    QObject::connect(handle(), &qw_seat::notify_start_drag, q, [this] (wlr_drag *drag) {
        on_start_drag(drag);
    });
}

void WSeatPrivate::updateCapabilities()
{
    uint32_t caps = 0;

    for (auto device : std::as_const(deviceList)) {
        if (device->type() == WInputDevice::Type::Keyboard) {
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        } else if (device->type() == WInputDevice::Type::Pointer) {
            caps |= WL_SEAT_CAPABILITY_POINTER;
        } else if (device->type() == WInputDevice::Type::Touch) {
            caps |= WL_SEAT_CAPABILITY_TOUCH;
        }
    }

    handle()->set_capabilities(caps);
}

void WSeatPrivate::attachInputDevice(WInputDevice *device)
{
    W_Q(WSeat);
    device->setSeat(q);
    auto qtDevice = QWlrootsIntegration::instance()->addInputDevice(device, name);
    Q_ASSERT(qtDevice);

    if (device->type() == WInputDevice::Type::Keyboard) {
        auto keyboard = qobject_cast<qw_keyboard*>(device->handle());

        /* We need to prepare an XKB keymap and assign it to the keyboard. This
         * assumes the defaults (e.g. layout = "us"). */
        struct xkb_rule_names rules = {};
        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
                                                           XKB_KEYMAP_COMPILE_NO_FLAGS);

        keyboard->set_keymap(keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        keyboard->set_repeat_info(25, 600);

        device->safeConnect(&qw_keyboard::notify_key, q, [this, device] (wlr_keyboard_key_event *event) {
            on_keyboard_key(event, device);
        });
        device->safeConnect(&qw_keyboard::notify_modifiers, q, [this, device] () {
            on_keyboard_modifiers(device);
        });
        handle()->set_keyboard(*keyboard);
    }
}

void WSeatPrivate::detachInputDevice(WInputDevice *device)
{
    if (cursor && device->type() == WInputDevice::Type::Pointer)
        cursor->detachInputDevice(device);

    if (device->type() == WInputDevice::Type::Touch) {
        qCDebug(qLcWlrTouch, "WSeat: detachTouchDevice %s", qPrintable(device->qtDevice()->name()));
        auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();
        device->removeAttachedData<WSeatPrivate::DeviceState>();
        delete state;
        touchDeviceList.removeOne(device);
    }

    [[maybe_unused]] bool ok = QWlrootsIntegration::instance()->removeInputDevice(device);
    Q_ASSERT(ok);
}

WSeat::WSeat(const QString &name)
    : WWrapObject(*new WSeatPrivate(this, name))
{

}

WSeat *WSeat::fromHandle(const qw_seat *handle)
{
    return handle->get_data<WSeat>();
}

// Some event filter related functions needs to get WSeat from QInputEvent,
// but the input event may come with a virtual device (e.g. created after a TTY
// switch by an intenal mechanism, like QHoverEvent). Thus we needs a special
// guarded getter.

// @return A pointer to WSeat if the event is valid, nullptr if not.
WSeat *WSeat::fromInputEvent(QInputEvent *event) {
    auto qtDevice = event->device();
    if (qtDevice->seatName().isEmpty()) {
        return nullptr;
    } else {
        auto device = WInputDevice::from(qtDevice);
        Q_ASSERT(device);
        auto seat = device->seat();
        Q_ASSERT(seat);
        return seat;
    }
}

qw_seat *WSeat::handle() const
{
    return d_func()->handle();
}

wlr_seat *WSeat::nativeHandle() const
{
    return d_func()->nativeHandle();
}

QString WSeat::name() const
{
    return d_func()->name;
}

void WSeat::setCursor(WCursor *cursor)
{
    W_D(WSeat);

    if (d->cursor == cursor)
        return;

    Q_ASSERT(!cursor || !cursor->seat());

    if (d->cursor) {
        for (auto i : std::as_const(d->deviceList)) {
            d->cursor->detachInputDevice(i);
        }

        d->cursor->setSeat(nullptr);
    }

    d->cursor = cursor;

    if (isValid() && cursor) {
        cursor->setSeat(this);

        for (auto i : std::as_const(d->deviceList)) {
            cursor->attachInputDevice(i);
        }
    }
}

WCursor *WSeat::cursor() const
{
    W_DC(WSeat);
    return d->cursor;
}

void WSeat::setCursorPosition(const QPointF &pos)
{
    W_D(WSeat);
    if (!cursor())
        return;

    cursor()->setPosition(pos);
    d->doMouseMove(cursor(), QPointingDevice::primaryPointingDevice(), QDateTime::currentMSecsSinceEpoch());
}

bool WSeat::setCursorPositionWithChecker(const QPointF &pos)
{
    W_D(WSeat);
    if (!cursor())
        return false;

    bool ok = cursor()->setPositionWithChecker(pos);
    d->doMouseMove(cursor(), QPointingDevice::primaryPointingDevice(), QDateTime::currentMSecsSinceEpoch());
    return ok;
}

WGlobal::CursorShape WSeat::requestedCursorShape() const
{
    W_DC(WSeat);

    if (d->cursorClient != d->nativeHandle()->pointer_state.focused_client) {
        qCritical("Focused client never set cursor shape nor surface, will fallback to `Default`");
        return WGlobal::CursorShape::Default;
    }

    return d->cursorShape;
}

WSurface *WSeat::requestedCursorSurface() const
{
    W_DC(WSeat);

    if (d->cursorClient == d->nativeHandle()->pointer_state.focused_client)
        return d->cursorSurface;
    return nullptr;
}

QPoint WSeat::requestedCursorSurfaceHotspot() const
{
    W_DC(WSeat);
    return d->cursorSurfaceHotspot;
}

WSurface *WSeat::requestedDragSurface() const
{
    W_DC(WSeat);
    return d->dragSurface;
}

void WSeat::attachInputDevice(WInputDevice *device)
{
    Q_ASSERT(!device->seat());
    W_D(WSeat);

    d->deviceList << device;

    if (isValid()) {
        d->attachInputDevice(device);
        d->updateCapabilities();

        if (d->cursor)
            // after d->attachInputDevice
            d->cursor->attachInputDevice(device);
    }

    if (device->type() == WInputDevice::Type::Touch) {
        qCDebug(qLcWlrTouch, "WSeat: registerTouchDevice %s", qPrintable(device->qtDevice()->name()));
        auto *state = new WSeatPrivate::DeviceState;
        device->setAttachedData<WSeatPrivate::DeviceState>(state);
        d->touchDeviceList << device;
    }
}

void WSeat::detachInputDevice(WInputDevice *device)
{
    W_D(WSeat);
    device->setSeat(nullptr);
    d->deviceList.removeOne(device);
    d->detachInputDevice(device);

    if (isValid())
        d->updateCapabilities();
}

inline static WSeat *getSeat(QInputEvent *event)
{
    auto inputDevice = WInputDevice::from(event->device());
    if (Q_UNLIKELY(!inputDevice))
        return nullptr;

    return inputDevice->seat();
}

bool WSeat::sendEvent(WSurface *target, QObject *shellObject, QObject *eventObject, QInputEvent *event)
{
    auto inputDevice = WInputDevice::from(event->device());
    if (Q_UNLIKELY(!inputDevice))
        return false;

    auto seat = inputDevice->seat();
    auto d = seat->d_func();

    auto eventState = d->getEventState(event);
    if (eventState)
        eventState->isAccepted = true;

    if (shellObject && d->eventFilter && d->eventFilter->beforeHandleEvent(seat, target, shellObject, eventObject, event))
        return true;

    event->accept();

    switch (event->type()) {
    case QEvent::HoverEnter: {
        auto e = static_cast<QHoverEvent*>(event);
        return d->doEnter(target, eventObject, e->position());
    }
    case QEvent::HoverLeave: {
        auto currentFocus = d->pointerFocusSurface();
        // Maybe this seat is grabbed by a xdg popup surface, so the surface of under mouse
        // can't take pointer focus, so if the eventObject is not pointerFocusEventObject,
        // we should don't do anything.
        if (d->pointerFocusEventObject != eventObject)
            break;
        auto nativeTarget = target->handle()->handle();
        Q_ASSERT(!currentFocus || d->oldPointerFocusSurface == nativeTarget || currentFocus == nativeTarget);
        d->doClearPointerFocus();
        break;
    }
    case QEvent::MouseButtonPress: {
        auto e = static_cast<QMouseEvent*>(event);
        Q_ASSERT(e->source() == Qt::MouseEventNotSynthesized);
        d->doNotifyButton(WCursor::toNativeButton(e->button()), WL_POINTER_BUTTON_STATE_PRESSED, event->timestamp());
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto e = static_cast<QMouseEvent*>(event);
        Q_ASSERT(e->source() == Qt::MouseEventNotSynthesized);
        d->doNotifyButton(WCursor::toNativeButton(e->button()), WL_POINTER_BUTTON_STATE_RELEASED, event->timestamp());
        break;
    }
    case QEvent::HoverMove: Q_FALLTHROUGH();
    case QEvent::MouseMove: {
        auto e = static_cast<QMouseEvent*>(event);
        Q_ASSERT(e->source() == Qt::MouseEventNotSynthesized);
        // When begin drag, the wlroots will grab pointer event for drag, and the pointer focus is nullptr.
        if (d->pointerFocusEventObject) {
            // received HoverEnter event of next eventObject before HoverLeave event of last eventObject,
            // so we should check the eventObject is still the same, if not, we should ignore this event
            if (d->pointerFocusEventObject != eventObject)
                break;
        }
        d->doNotifyMotion(target, eventObject, e->position(), e->timestamp());
        break;
    }
    case QEvent::Wheel: {
        if (auto we = dynamic_cast<WSeatWheelEvent*>(event)) {
            d->doNotifyAxis(static_cast<wl_pointer_axis_source>(we->wlrSource()),
                            we->orientation(), we->relativeDirection(),
                            we->wlrDelta(),
                            -(we->angleDelta().x()+we->angleDelta().y()), // one of them must be 0, restore to wayland direction here.
                            we->timestamp());
        } else {
            qWarning("An Wheel event was received that was not sent by wlroot and will be ignored");
        }
        break;
    }
    case QEvent::KeyPress: {
        auto e = static_cast<QKeyEvent*>(event);
        if (!e->isAutoRepeat())
            d->doNotifyKey(inputDevice, e->nativeVirtualKey(), WL_KEYBOARD_KEY_STATE_PRESSED, e->timestamp());
        break;
    }
    case QEvent::KeyRelease: {
        auto e = static_cast<QKeyEvent*>(event);
        if (!e->isAutoRepeat())
            d->doNotifyKey(inputDevice, e->nativeVirtualKey(), WL_KEYBOARD_KEY_STATE_RELEASED, e->timestamp());
        break;
    }
    case QEvent::TouchBegin: Q_FALLTHROUGH();
    case QEvent::TouchUpdate: Q_FALLTHROUGH();
    case QEvent::TouchEnd:
    {
        auto e = static_cast<QTouchEvent*>(event);
        for (const QEventPoint &touchPoint : std::as_const(e->points())) {
            d->doNotifyFullTouchEvent(target, touchPoint.id(), touchPoint.position(), touchPoint.state(), e->timestamp());
        }
        break;
    }
    case QEvent::TouchCancel: {
        auto e = static_cast<QTouchEvent*>(event);
        d->doTouchNotifyCancel(WInputDevice::from(e->device()));
        break;
    }
    case QEvent::NativeGesture: {
        if (!d->gesture)
            break;
        auto e = static_cast<WGestureEvent*>(event);
        switch (e->gestureType()) {
            case Qt::NativeGestureType::BeginNativeGesture:
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                d->gesture->send_swipe_begin(d->nativeHandle(), e->timestamp(), e->fingerCount());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::PinchGesture)
                d->gesture->send_pinch_begin(d->nativeHandle(), e->timestamp(), e->fingerCount());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                    d->gesture->send_hold_begin(d->nativeHandle(), e->timestamp(), e->fingerCount());
                break;
            case Qt::NativeGestureType::PanNativeGesture:
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                    d->gesture->send_swipe_update(d->nativeHandle(), e->timestamp(), e->delta().x(), e->delta().y());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::PinchGesture)
                    d->gesture->send_pinch_update(d->nativeHandle(), e->timestamp(), e->delta().x(), e->delta().y(), d->lastScale, 0);
                break;
            case Qt::NativeGestureType::ZoomNativeGesture:
                d->gesture->send_pinch_update(d->nativeHandle(), e->timestamp(), e->delta().x(), e->delta().y(), d->lastScale, 0);
                break;
            case Qt::NativeGestureType::RotateNativeGesture:
                d->gesture->send_pinch_update(d->nativeHandle(), e->timestamp(), e->delta().x(), e->delta().y(), d->lastScale, e->value());
                break;
            case Qt::NativeGestureType::EndNativeGesture:
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                    d->gesture->send_swipe_end(d->nativeHandle(), e->timestamp(), e->cancelled());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::PinchGesture)
                    d->gesture->send_pinch_end(d->nativeHandle(), e->timestamp(), e->cancelled());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                    d->gesture->send_hold_end(d->nativeHandle(), e->timestamp(), e->cancelled());
                break;
            default:
                break;
        }
        break;
    }
    default:
        event->ignore();
        return false;
    }

    if (!shellObject || !d->eventFilter)
        return true;

    switch (event->type()) {
    case QEvent::MouseButtonPress: Q_FALLTHROUGH();
    case QEvent::MouseButtonRelease: Q_FALLTHROUGH();
    case QEvent::HoverMove: Q_FALLTHROUGH();
    case QEvent::MouseMove: {
        // Maybe this event is eat by the event grabber
        if (target != seat->pointerFocusSurface())
            return true;
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        // Maybe this event is eat by the event grabber
        if (target != seat->keyboardFocusSurface())
            return true;
        break;
    default:
        break;
    }
    }

    d->eventFilter->afterHandleEvent(seat, target, shellObject, eventObject, event);

    return true;
}

WSeat *WSeat::get(QInputEvent *event)
{
    auto inputDevice = WInputDevice::from(event->device());
    return inputDevice ? inputDevice->seat() : nullptr;
}

WSurface *WSeat::pointerFocusSurface() const
{
    W_DC(WSeat);
    if (auto fs = d->pointerFocusSurface())
        return WSurface::fromHandle(qw_surface::from(fs));
    return nullptr;
}

void WSeat::setKeyboardFocusSurface(WSurface *surface)
{
    W_D(WSeat);

    if (d->m_keyboardFocusSurface == surface)
        return;

    d->m_keyboardFocusSurface = surface;
    if (isValid())
        d->doSetKeyboardFocus(surface ? surface->handle() : nullptr);

    Q_EMIT keyboardFocusSurfaceChanged();
}

WSurface *WSeat::keyboardFocusSurface() const
{
    W_DC(WSeat);
    return d->m_keyboardFocusSurface;
}

void WSeat::clearKeyboardFocusSurface()
{
    W_D(WSeat);
    d->doSetKeyboardFocus(nullptr);
}

void WSeat::setKeyboardFocusWindow(QWindow *window)
{
    W_D(WSeat);
    d->focusWindow = window;
}

QWindow *WSeat::keyboardFocusWindow() const
{
    W_DC(WSeat);
    return d->focusWindow;
}

void WSeat::clearKeyboardFocusWindow()
{
    W_D(WSeat);
    d->focusWindow = nullptr;
}

WInputDevice *WSeat::keyboard() const
{
    W_DC(WSeat);
    auto w_keyboard = d->handle()->get_keyboard();
    if (w_keyboard) {
        auto q_keyboard = qw_keyboard::from(w_keyboard);
        auto device = WInputDevice::fromHandle(q_keyboard);
        Q_ASSERT(device);
        return device;
    } else {
        return nullptr;
    }
}

void WSeat::setKeyboard(WInputDevice *newKeyboard)
{
    W_D(WSeat);
    if (newKeyboard == keyboard())
        return;
    d->handle()->set_keyboard(*qobject_cast<qw_keyboard *>(newKeyboard->handle()));
    Q_EMIT this->keyboardChanged();
}

bool WSeat::alwaysUpdateHoverTarget() const
{
    W_DC(WSeat);
    return d->alwaysUpdateHoverTarget;
}

void WSeat::setAlwaysUpdateHoverTarget(bool newIgnoreSurfacePointerEventExclusiveGrabber)
{
    W_D(WSeat);
    if (d->alwaysUpdateHoverTarget == newIgnoreSurfacePointerEventExclusiveGrabber)
        return;
    d->alwaysUpdateHoverTarget = newIgnoreSurfacePointerEventExclusiveGrabber;

    if (d->alwaysUpdateHoverTarget) {
        for (WInputDevice *device : std::as_const(d->deviceList)) {
            // Qt will auto grab the pointer event for QQuickItem when mouse pressed
            // until mouse released. But we want always update the HoverEnter/Leave's
            // WSurfaceItem between drag move.
            if (device->exclusiveGrabber() == device->hoverTarget())
                device->setExclusiveGrabber(nullptr);
        }
    } else {
        for (WInputDevice *device : std::as_const(d->deviceList)) {
            if (!device->exclusiveGrabber()) {
                // Restore
                device->setExclusiveGrabber(device->hoverTarget());
            }
        }
    }

    Q_EMIT alwaysUpdateHoverTargetChanged();
}

void WSeat::notifyMotion(WCursor *cursor, WInputDevice *device, uint32_t timestamp)
{
    W_D(WSeat);

    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    d->doMouseMove(cursor, qwDevice, timestamp);
}

void WSeat::notifyButton(WCursor *cursor, WInputDevice *device, Qt::MouseButton button,
                         wl_pointer_button_state_t state, uint32_t timestamp)
{
    W_D(WSeat);

    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);

    QWindow *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();
    auto et = state == WLR_BUTTON_PRESSED ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;

    QMouseEvent e(et, local, global, button,
                  cursor->state(), d->keyModifiers, qwDevice);
    if (et == QEvent::MouseButtonPress)
        Q_ASSERT(e.isBeginEvent());
    else
        Q_ASSERT(e.isEndEvent());
    e.setTimestamp(timestamp);

    if (w)
        QCoreApplication::sendEvent(w, &e);
}

void WSeat::notifyAxis(WCursor *cursor, WInputDevice *device, wl_pointer_axis_source_t source,
                       Qt::Orientation orientation, wl_pointer_axis_relative_direction_t rd,
                       double delta, int32_t delta_discrete, uint32_t timestamp)
{
    W_D(WSeat);

    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);

    QWindow *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    // Refer to https://github.com/qt/qtwayland/blob/774c0be247bd04362fc7713919ac151c44e34ced/src/client/qwaylandinputdevice.cpp#L1089
    // The direction in Qt event is in the opposite direction of wayland one, generate a event identical to Qt's direction.
    QPoint angleDelta = orientation == Qt::Horizontal ? QPoint(-delta_discrete, 0) : QPoint(0, -delta_discrete);
    WSeatWheelEvent e(static_cast<wl_pointer_axis_source>(source), delta, orientation,
                      static_cast<wl_pointer_axis_relative_direction>(rd),
                      local, global, QPoint(), angleDelta, Qt::NoButton, d->keyModifiers,
                      Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized, qwDevice);
    e.setTimestamp(timestamp);

    if (w) {
        QCoreApplication::sendEvent(w, &e);
    } else {
        d->doNotifyAxis(static_cast<wl_pointer_axis_source>(source), orientation,
                        static_cast<wl_pointer_axis_relative_direction>(rd),
                        delta, delta_discrete, timestamp);
    }
}

void WSeat::notifyFrame(WCursor *cursor)
{
    Q_UNUSED(cursor);
    W_D(WSeat);
    d->doNotifyFrame();
}

void WSeat::notifyGestureBegin(WCursor *cursor, WInputDevice *device, uint32_t time_msec, uint32_t fingers, WGestureEvent::WLibInputGestureType libInputGestureType)
{
    W_D(WSeat);
    if (d->gestureActive) {
        qCWarning(qLcWlrGestureEvents) << "Unexpected GestureBegin while already active";
    }
    d->gestureActive = true;
    d->gestureFingers = fingers;
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    auto *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    WGestureEvent e(libInputGestureType, Qt::NativeGestureType::BeginNativeGesture, qwDevice,
                    fingers, local, local, global, 0, QPointF(0, 0));
    if (w)
        QCoreApplication::sendEvent(w, &e);
}

void WSeat::notifyGestureUpdate(WCursor *cursor, WInputDevice *device, uint32_t time_msec, const QPointF &delta, double scale, double rotation, WGestureEvent::WLibInputGestureType libInputGestureType)
{
    W_D(WSeat);
    if (!d->gestureActive) {
        qCWarning(qLcWlrGestureEvents) << "Unexpected GestureUpdate while not begin";
        return;
    }
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    auto *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();
    if (!delta.isNull()) {
        WGestureEvent e(libInputGestureType, Qt::NativeGestureType::PanNativeGesture, qwDevice,
                        d->gestureFingers, local, local, global, 0, delta);
        if (w)
            QCoreApplication::sendEvent(w, &e);
    }
    if (rotation != 0) {
        WGestureEvent e(libInputGestureType, Qt::NativeGestureType::RotateNativeGesture, qwDevice,
                        d->gestureFingers, local, local, global, rotation, delta);
        if (w)
            QCoreApplication::sendEvent(w, &e);
    }
    if (scale != 0) {
        WGestureEvent e(libInputGestureType, Qt::NativeGestureType::ZoomNativeGesture, qwDevice,
                        d->gestureFingers, local, local, global, rotation, delta);
        if (w)
            QCoreApplication::sendEvent(w, &e);
        d->lastScale = scale;
    }
}

void WSeat::notifyGestureEnd(WCursor *cursor, WInputDevice *device, uint32_t time_msec, bool cancelled, WGestureEvent::WLibInputGestureType libInputGestureType)
{
    W_D(WSeat);
    if (!d->gestureActive) {
        qCWarning(qLcWlrGestureEvents) << "Unexpected GestureEnd while not begin";
        return;
    }
    d->gestureActive = false;
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    auto *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    WGestureEvent e(libInputGestureType, Qt::NativeGestureType::EndNativeGesture, qwDevice,
                    d->gestureFingers, local, local, global, 0, QPointF(0, 0));
    if (w)
        QCoreApplication::sendEvent(w, &e);
}

void WSeat::notifyHoldBegin(WCursor *cursor, WInputDevice *device, uint32_t time_msec, uint32_t fingers)
{
    W_D(WSeat);
    if (d->gestureActive) {
        qCWarning(qLcWlrGestureEvents) << "Unexpected HoldBegin while already active";
    }
    d->gestureActive = true;
    d->gestureFingers = fingers;
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    auto *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    WGestureEvent e(WGestureEvent::HoldGesture, Qt::NativeGestureType::BeginNativeGesture, qwDevice,
                    d->gestureFingers, local, local, global, 0, QPointF(0, 0));
    e.setTimestamp(time_msec);
    if (w)
        QCoreApplication::sendEvent(w, &e);
}
void WSeat::notifyHoldEnd(WCursor *cursor, WInputDevice *device, uint32_t time_msec, bool cancelled)
{
    W_D(WSeat);
    if (!d->gestureActive) {
        qCWarning(qLcWlrGestureEvents) << "Unexpected HoldEnd while not begin";
        return;
    }
    d->gestureActive = false;
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    auto *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    WGestureEvent e(WGestureEvent::HoldGesture, Qt::NativeGestureType::EndNativeGesture, qwDevice,
                    d->gestureFingers, local, local, global, 0, QPointF(0, 0));
    e.setTimestamp(time_msec);
    e.setCancelled(cancelled);
    if (w)
        QCoreApplication::sendEvent(w, &e);
}

// deal with touch event form wlr_cursor

void WSeat::notifyTouchDown(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec)
{
    W_D(WSeat);
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);
    const QPointF &globalPos = cursor->position();

    auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();

    QWindowSystemInterface::TouchPoint *tp = state->point(touch_id);
    if (Q_UNLIKELY(tp)) {
        // The touch_id may be reused by a new Down event after the Up event
        // There may not be a Frame event after the last Up.
        // Manually create a Frame event to prevent touch_id conflicts in DeviceState

        if (Q_LIKELY(tp->state == QEventPoint::Released)) {
            // Only the Released Point can be removed in next frame event.
            notifyTouchFrame(cursor);
        }

        if (state->point(touch_id) != nullptr) {
            qWarning("Inconsistent touch state, (got 'Down' But touch_id(%d) is not released", touch_id);
        }
    }

    QWindowSystemInterface::TouchPoint newTp;
    newTp.id = touch_id;
    newTp.state = QEventPoint::Pressed;
    // default value of newTp.area keep same with qlibinputtouch
    // Ref: https://github.com/qt/qtbase/blob/6.5/src/platformsupport/input/libinput/qlibinputtouch.cpp#L114
    newTp.area = QRect(0, 0, 8, 8);
    newTp.area.moveCenter(globalPos);
    state->m_points.append(newTp);
    qCDebug(qLcWlrTouchEvents) << "Touch down form device: " << qwDevice->name()
                               << ", touch id: " << touch_id
                               << ", at position" << globalPos;
}

void WSeat::notifyTouchMotion(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec)
{

    W_DC(WSeat);
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);

    const QPointF &globalPos = cursor->position();
    auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();
    QWindowSystemInterface::TouchPoint *tp = state->point(touch_id);

    if (Q_LIKELY(tp)) {
        auto tmpState = QEventPoint::Updated;
        if (tp->area.center() == globalPos)
            tmpState = QEventPoint::Stationary;
        else
            tp->area.moveCenter(globalPos);
        // 'down' may be followed by 'motion' within the same "frame".
        // Handle this by compressing and keeping the Pressed state until the 'frame'.
        if (tp->state != QEventPoint::Pressed && tp->state != QEventPoint::Released)
            tp->state = tmpState;
        qCDebug(qLcWlrTouchEvents) << "Touch move form device: " << qwDevice->name()
                                   << ", touch id: " << touch_id
                                   << ", to position: " << globalPos
                                   << ", state of the point: " << tp->state;
    } else {
        qWarning("Inconsistent touch state (got 'Motion' without 'Down'");
    }
}

void WSeat::notifyTouchUp(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec)
{
    W_DC(WSeat);
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);

    auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();
    QWindowSystemInterface::TouchPoint *tp = state->point(touch_id);

    if (Q_LIKELY(tp)) {
        tp->state = QEventPoint::Released;
        // There may not be a Frame event after the last Up. Work this around.
        // IF All Points has Released, Send a Frame event immediately
        // Ref: https://github.com/qt/qtbase/blob/6.5/src/platformsupport/input/libinput/qlibinputtouch.cpp#L150
        QEventPoint::States s;
        for (const auto &point : std::as_const(state->m_points)) {
            s |= point.state;
        }
        qCDebug(qLcWlrTouchEvents) << "Touch up form device: " << qwDevice->name()
                                   << ", touch id: " << tp->id
                                   << ", at position: " << tp->area.center()
                                   << ", state of all points of this device: " << s;

        if (s == QEventPoint::Released)
            notifyTouchFrame(cursor);
        else
            qCDebug(qLcWlrTouchEvents) << "waiting for all points to be released";
    } else {
        qWarning("Inconsistent touch state (got 'Up' without 'Down'");
    }
}

void WSeat::notifyTouchCancel(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec)
{
    W_DC(WSeat);
    auto qwDevice = qobject_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);

    auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();
    for (int i = 0; i < state->m_points.size(); ++i) {
        auto point = state->point(touch_id);
        Q_ASSERT(point);
        point->state = static_cast<QEventPoint::State>(WEvent::PointCancelled);
    }

    qCDebug(qLcWlrTouchEvents) << "Touch cancel for device: " << qwDevice->name()
        << ", discard the following state: " << state->m_points;

    if (cursor->eventWindow()) {
        QWindowSystemInterface::handleTouchCancelEvent(cursor->eventWindow(), time_msec, qwDevice, d->keyModifiers);
    }
}

void WSeat::notifyTouchFrame(WCursor *cursor)
{
    W_D(WSeat);
    Q_UNUSED(cursor);
    for (auto *device: std::as_const(d->touchDeviceList)) {
        d->doNotifyTouchFrame(device);
    }
}

void WSeat::setCursorShape(wlr_seat_client *client, WGlobal::CursorShape shape)
{
    W_D(WSeat);
    if (client != d->nativeHandle()->pointer_state.focused_client)
        return;
    d->cursorShape = shape;
    d->cursorClient = client;

    if (d->cursorSurface)
        d->cursorSurface->safeDeleteLater();

    Q_EMIT requestCursorShape(shape);
}

WSeatEventFilter *WSeat::eventFilter() const
{
    W_DC(WSeat);
    return d->eventFilter.data();
}

void WSeat::setEventFilter(WSeatEventFilter *filter)
{
    W_D(WSeat);
    Q_ASSERT(!filter || !d->eventFilter);
    d->eventFilter = filter;
}

void WSeat::create(WServer *server)
{
    W_D(WSeat);
    // destroy follow display
    const auto name = d->name.toUtf8();
    m_handle = qw_seat::create(*server->handle(), name.constData());
    initHandle(d->handle());
    d->handle()->set_data(this, this);
    d->connect();

    for (auto i : std::as_const(d->deviceList)) {
        d->attachInputDevice(i);

        if (d->cursor)
            d->cursor->attachInputDevice(i);
    }

    if (!qEnvironmentVariableIsSet("WAYLIB_DISABLE_GESTURE"))
        d->gesture = qw_pointer_gestures_v1::create(*server->handle());

    d->updateCapabilities();

    if (d->cursor)
        // after d->attachInputDevice
        d->cursor->setSeat(this);

    if (d->m_keyboardFocusSurface)
        d->doSetKeyboardFocus(d->m_keyboardFocusSurface->handle());
}

void WSeat::destroy(WServer *)
{
    W_D(WSeat);

    for (auto i : std::as_const(d->deviceList)) {
        i->setSeat(nullptr);
    }

    d->deviceList.clear();

    // Need not call the DCursor::detachInputDevice on destroy WSeat, so do
    // call the detachCursor at clear the deviceList after.
    if (d->cursor)
        setCursor(nullptr);

    if (m_handle) {
        d->handle()->set_data(nullptr, nullptr);
        m_handle = nullptr;
    }
}

wl_global *WSeat::global() const
{
    W_D(const WSeat);
    return d->nativeHandle()->global;
}

QByteArrayView WSeat::interfaceName() const
{
    return wl_seat_interface.name;
}

bool WSeat::filterEventBeforeDisposeStage(QWindow *targetWindow, QInputEvent *event)
{
    W_D(WSeat);

    d->addEventState(event);

    if (Q_UNLIKELY(d->alwaysUpdateHoverTarget) && event->isPointerEvent()) {
        auto pe = static_cast<QPointerEvent*>(event);
        if (pe->isEndEvent()) {
            auto device = WInputDevice::from(event->device());
            if (!device->exclusiveGrabber()) {
                // Restore the grabber, See alwaysUpdateHoverTarget
                device->setExclusiveGrabber(device->hoverTarget());
            }
        }
    }

    if (Q_UNLIKELY(d->eventFilter)) {
        if (d->eventFilter->beforeDisposeEvent(this, targetWindow, event)) {
            if (event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove) {
                // ###: Qt need 'lastMousePosition' to synchronous hover in
                // QQuickDeliveryAgentPrivate::flushFrameSynchronousEvents,
                // If the mouse move event is not send to QQuickWindow, maybe
                // you will get a bad QHoverEnter and QHoverLeave event in future,
                // because the QQuickDeliveryAgent can't get the real last mouse
                // position, the QQuickWindowPrivate::lastMousePosition is error.
                if (QQuickWindow *qw = qobject_cast<QQuickWindow*>(targetWindow)) {
                    Q_ASSERT(event->isSinglePointEvent());
                    const auto pos = static_cast<QSinglePointEvent*>(event)->position();
                    QQuickWindowPrivate::get(qw)->deliveryAgentPrivate()->lastMousePosition = pos;
                }
            }

            return true;
        }
    }

    return false;
}

bool WSeat::filterEventBeforeDisposeStage(QQuickItem *target, QInputEvent *event)
{
    if (event->type() == QEvent::HoverEnter) {
        auto ie = WInputDevice::from(event->device());
        ie->setHoverTarget(target);
    } else if (event->type() == QEvent::HoverLeave) {
        auto ie = WInputDevice::from(event->device());
        if (ie->hoverTarget() == target)
            ie->setHoverTarget(nullptr);
    }

    return false;
}

bool WSeat::filterEventAfterDisposeStage(QWindow *targetWindow, QInputEvent *event)
{
    W_D(WSeat);

    int eventStateIndex = d->indexOfEventState(event);
    Q_ASSERT(eventStateIndex >= 0);

    if (event->isAccepted() || d->pendingEvents.at(eventStateIndex).isAccepted) {
        d->pendingEvents.removeAt(eventStateIndex);

        if (Q_UNLIKELY(d->alwaysUpdateHoverTarget) && event->isPointerEvent()) {
            auto pe = static_cast<QPointerEvent*>(event);

            // Qt will auto grab the pointer event for QQuickItem when mouse pressed
            // until mouse released. But we want always update the HoverEnter/Leave's
            // WSurfaceItem between drag move.
            if (pe->isBeginEvent()) {
                auto ie = WInputDevice::from(event->device());
                if (ie->exclusiveGrabber() == ie->hoverTarget())
                    ie->setExclusiveGrabber(nullptr);
            }
        }

        return false;
    }

    d->pendingEvents[eventStateIndex].isAccepted = true;
    bool ok = filterUnacceptedEvent(targetWindow, event);

    d->pendingEvents.removeAt(eventStateIndex);

    return ok;
}

bool WSeat::filterUnacceptedEvent(QWindow *targetWindow, QInputEvent *event)
{
    W_D(WSeat);

    switch (event->type()) {
    // Maybe this seat has grabbed in wlroots, should send these events to graber.
    case QEvent::MouseButtonPress: Q_FALLTHROUGH();
    case QEvent::MouseButtonRelease: Q_FALLTHROUGH();
    case QEvent::HoverMove: Q_FALLTHROUGH();
    case QEvent::MouseMove:
        if (static_cast<QMouseEvent*>(event)->source() != Qt::MouseEventNotSynthesized)
            return false;
        if (d->handle()->pointer_has_grab())
            return sendEvent(nullptr, nullptr, nullptr, event);
        break;
    case QEvent::KeyPress: Q_FALLTHROUGH();
    case QEvent::KeyRelease:
        if (d->handle()->keyboard_has_grab())
            return sendEvent(nullptr, nullptr, nullptr, event);
        break;
        // TODO: Must send the touch events to touch grabber, but the touch
        // event need a non-NULL surface object, we can create a wl_client
        // in a new thread and add a exclusive wl_surface to receive these events.
        //    case QEvent::TouchBegin: Q_FALLTHROUGH();
        //    case QEvent::TouchCancel: Q_FALLTHROUGH();
        //    case QEvent::TouchEnd: Q_FALLTHROUGH();
        //    case QEvent::TouchUpdate: Q_FALLTHROUGH();
        //        if (d->handle()->touchHasGrab())
        //            return sendEvent(nullptr, nullptr, nullptr, event);
        //        break;
    default:
        break;
    }

    if (d->eventFilter && d->eventFilter->unacceptedEvent(this, targetWindow, event))
        return true;

    return false;
}

WSeatEventFilter::WSeatEventFilter(QObject *parent)
    : QObject(parent)
{

}

bool WSeatEventFilter::beforeHandleEvent(WSeat *, WSurface *, QObject *,
                                         QObject *, QInputEvent *)
{
    return false;
}

bool WSeatEventFilter::afterHandleEvent(WSeat *, WSurface *, QObject *,
                                        QObject *, QInputEvent *)
{
    return false;
}

bool WSeatEventFilter::beforeDisposeEvent(WSeat *, QWindow *, QInputEvent *)
{
    return false;
}

bool WSeatEventFilter::unacceptedEvent(WSeat *, QWindow *, QInputEvent *)
{
    return false;
}

WAYLIB_SERVER_END_NAMESPACE
