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

#include <QQuickWindow>
#include <QGuiApplication>
#include <QQuickItem>
#include <QDebug>
#include <QTimer>

#include <qpa/qwindowsysteminterface.h>
#include <private/qxkbcommon_p.h>
#include <private/qquickwindow_p.h>
#include <private/qquickdeliveryagent_p_p.h>

extern "C" {
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#define static
#include <wlr/types/wlr_cursor.h>
#undef static
#include <wlr/types/wlr_xdg_shell.h>
}

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
class WSeatWheelEvent : public QWheelEvent {
public:
    WSeatWheelEvent(wlr_axis_source_t wlr_source, double wlr_delta,
                    const QPointF &pos, const QPointF &globalPos, QPoint pixelDelta, QPoint angleDelta,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, Qt::ScrollPhase phase,
                    bool inverted, Qt::MouseEventSource source = Qt::MouseEventNotSynthesized,
                    const QPointingDevice *device = QPointingDevice::primaryPointingDevice())
        : QWheelEvent(pos, globalPos, pixelDelta, angleDelta, buttons, modifiers, phase, inverted, source, device)
        , m_wlrSource(wlr_source)
        , m_wlrDelta(wlr_delta)
    {

    }

    inline wlr_axis_source_t wlrSource() const { return m_wlrSource; }
    inline double wlrDelta() const { return m_wlrDelta; }

protected:
    wlr_axis_source_t m_wlrSource;
    double m_wlrDelta;
};
#endif

class WSeatPrivate : public WWrapObjectPrivate
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
            auto rawdevice = qobject_cast<QWKeyboard*>(WInputDevice::from(m_repeatKey->device())->handle())->handle();
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

        for (auto device : deviceList)
            detachInputDevice(device);
    }

    inline QWSeat *handle() const {
        return q_func()->nativeInterface<QWSeat>();
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

        handle()->pointerNotifyMotion(timestamp, localPos.x(), localPos.y());
        return true;
    }
    inline bool doNotifyButton(uint32_t button, wlr_button_state state, uint32_t timestamp) {
        handle()->pointerNotifyButton(timestamp, button, state);
        return true;
    }
    static inline wlr_axis_orientation fromQtHorizontal(Qt::Orientation o) {
        return o == Qt::Horizontal ? WLR_AXIS_ORIENTATION_HORIZONTAL
                                   : WLR_AXIS_ORIENTATION_VERTICAL;
    }
    inline bool doNotifyAxis(wlr_axis_source source, Qt::Orientation orientation,
                             double delta, int32_t delta_discrete, uint32_t timestamp) {
        if (!pointerFocusSurface())
            return false;

        handle()->pointerNotifyAxis(timestamp, fromQtHorizontal(orientation), delta, delta_discrete, source);
        return true;
    }
    inline void doNotifyFrame() {
        handle()->pointerNotifyFrame();
    }
    inline bool doEnter(WSurface *surface, QObject *eventObject, const QPointF &position) {
        auto tmp = oldPointerFocusSurface;
        oldPointerFocusSurface = handle()->handle()->pointer_state.focused_surface;
        handle()->pointerNotifyEnter(surface->handle(), position.x(), position.y());
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
        handle()->pointerNotifyClearFocus();
        Q_ASSERT(!handle()->handle()->pointer_state.focused_surface);
        if (cursor) // reset cursur from QCursor resource, the last cursor is from wlr_surface
            cursor->setCursor(cursor->cursor());
    }
    inline void doSetKeyboardFocus(QWSurface *surface) {
        if (surface) {
            handle()->keyboardEnter(surface, nullptr, 0, nullptr);
        } else {
            handle()->keyboardClearFocus();
        }
    }
    inline void doTouchNotifyDown(WSurface *surface, uint32_t time_msec, int32_t touch_id, const QPointF &pos) {
        handle()->touchNotifyDown(surface->handle(), time_msec, touch_id, pos.x(), pos.y());
    }
    inline void doTouchNotifyMotion(uint32_t time_msec, int32_t touch_id, const QPointF &pos) {
        handle()->touchNotifyMotion(time_msec, touch_id, pos.x(), pos.y());
    }
    inline void doTouchNotifyUp(uint32_t time_msec, int32_t touch_id) {
        handle()->touchNotifyUp(time_msec, touch_id);
    }
    inline void doTouchNotifyCancel(QWSurface *surface) {
        handle()->touchNotifyCancel(surface);
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
        }
        handle()->touchNotifyFrame();
    }

    // for keyboard event
    inline bool doNotifyKey(WInputDevice *device, uint32_t keycode, uint32_t state, uint32_t timestamp) {
        if (!keyboardFocusSurface())
            return false;

        q_func()->setKeyboard(device);
        /* Send modifiers to the client. */
        this->handle()->keyboardNotifyKey(timestamp, keycode, state);
        return true;
    }
    inline bool doNotifyModifiers(WInputDevice *device) {
        if (!keyboardFocusSurface())
            return false;

        auto keyboard = qobject_cast<QWKeyboard*>(device->handle());
        q_func()->setKeyboard(device);
        /* Send modifiers to the client. */
        this->handle()->keyboardNotifyModifiers(&keyboard->handle()->modifiers);
        return true;
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
    QWPointerGesturesV1 *gesture = nullptr;
    QVector<WInputDevice*> deviceList;
    QVector<WInputDevice*> touchDeviceList;
    QPointer<WSeatEventFilter> eventFilter;
    QPointer<QWindow> focusWindow;
    QPointer<QObject> pointerFocusEventObject;
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
        auto *surface = event->surface ? QWSurface::from(event->surface) : nullptr;
        cursor->setSurface(surface, QPoint(event->hotspot_x, event->hotspot_y));
    }
}

void WSeatPrivate::on_request_set_selection(wlr_seat_request_set_selection_event *event)
{
    handle()->setSelection(event->source, event->serial);
}

void WSeatPrivate::on_request_set_primary_selection(wlr_seat_request_set_primary_selection_event *event)
{
    wlr_seat_set_primary_selection(nativeHandle(), event->source, event->serial);
}

void WSeatPrivate::on_request_start_drag(wlr_seat_request_start_drag_event *event)
{
    if (handle()->validatePointerGrabSerial(QWSurface::from(event->origin), event->serial)) {
        wlr_seat_start_pointer_drag(nativeHandle(), event->drag, event->serial);
        return;
    }

    struct wlr_touch_point *point;
    if (handle()->validateTouchGrabSerial(QWSurface::from(event->origin), event->serial, &point)) {
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
        auto *surface = QWSurface::from(drag->icon->surface);
        auto *wsurface = new WSurface(surface, surface);
        cursor->setDragSurface(wsurface);
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
    auto keyboard = qobject_cast<QWKeyboard*>(device->handle());

    auto code = event->keycode + 8; // map to wl_keyboard::keymap_format::keymap_format_xkb_v1
    auto et = event->state == WL_KEYBOARD_KEY_STATE_PRESSED ? QEvent::KeyPress : QEvent::KeyRelease;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(keyboard->handle()->xkb_state, code);
    int qtkey = QXkbCommon::keysymToQtKey(sym, keyModifiers, keyboard->handle()->xkb_state, code);
    const QString &text = QXkbCommon::lookupString(keyboard->handle()->xkb_state, code);

    QKeyEvent e(et, qtkey, keyModifiers, code, event->keycode, keyboard->getModifiers(),
                text, false, 1, device->qtDevice());
    e.setTimestamp(event->time_msec);

    if (focusWindow) {
        handleKeyEvent(e);
        if (et == QEvent::KeyPress && xkb_keymap_key_repeats(keyboard->handle()->keymap, code)) {
            if (m_repeatKey) {
                m_repeatTimer.stop();
            }
            m_repeatKey = std::make_unique<QKeyEvent>(et, qtkey, keyModifiers, code, event->keycode, keyboard->getModifiers(),
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
    auto keyboard = qobject_cast<QWKeyboard*>(device->handle());
    keyModifiers = QXkbCommon::modifiers(keyboard->handle()->xkb_state);
    doNotifyModifiers(device);
}

void WSeatPrivate::connect()
{
    W_Q(WSeat);
    QObject::connect(handle(), &QWSeat::destroyed, q, [this] {
        on_destroy();
    });
    QObject::connect(handle(), &QWSeat::requestSetCursor, q, [this] (wlr_seat_pointer_request_set_cursor_event *event) {
        on_request_set_cursor(event);
    });
    QObject::connect(handle(), &QWSeat::requestSetSelection, q, [this] (wlr_seat_request_set_selection_event *event) {
        on_request_set_selection(event);
    });
    QObject::connect(handle(), &QWSeat::requestSetPrimarySelection, q, [this] (wlr_seat_request_set_primary_selection_event *event) {
        on_request_set_primary_selection(event);
    });
    QObject::connect(handle(), &QWSeat::requestStartDrag, q, [this] (wlr_seat_request_start_drag_event *event) {
        on_request_start_drag(event);
    });
    QObject::connect(handle(), &QWSeat::startDrag, q, [this] (wlr_drag *drag) {
        on_start_drag(drag);
    });
}

void WSeatPrivate::updateCapabilities()
{
    uint32_t caps = 0;

    Q_FOREACH(auto device, deviceList) {
        if (device->type() == WInputDevice::Type::Keyboard) {
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        } else if (device->type() == WInputDevice::Type::Pointer) {
            caps |= WL_SEAT_CAPABILITY_POINTER;
        } else if (device->type() == WInputDevice::Type::Touch) {
            caps |= WL_SEAT_CAPABILITY_TOUCH;
        }
    }

    handle()->setCapabilities(caps);
}

void WSeatPrivate::attachInputDevice(WInputDevice *device)
{
    W_Q(WSeat);
    device->setSeat(q);
    auto qtDevice = QWlrootsIntegration::instance()->addInputDevice(device, name);
    Q_ASSERT(qtDevice);

    if (device->type() == WInputDevice::Type::Keyboard) {
        auto keyboard = qobject_cast<QWKeyboard*>(device->handle());

        /* We need to prepare an XKB keymap and assign it to the keyboard. This
         * assumes the defaults (e.g. layout = "us"). */
        struct xkb_rule_names rules = {};
        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
                                                           XKB_KEYMAP_COMPILE_NO_FLAGS);

        keyboard->setKeymap(keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        keyboard->setRepeatInfo(25, 600);

        device->safeConnect(&QWKeyboard::key, q, [this, device] (wlr_keyboard_key_event *event) {
            on_keyboard_key(event, device);
        });
        device->safeConnect(&QWKeyboard::modifiers, q, [this, device] () {
            on_keyboard_modifiers(device);
        });
        handle()->setKeyboard(keyboard);
    }
}

void WSeatPrivate::detachInputDevice(WInputDevice *device)
{
    if (cursor && device->type() == WInputDevice::Type::Pointer)
        cursor->detachInputDevice(device);

    [[maybe_unused]] bool ok = QWlrootsIntegration::instance()->removeInputDevice(device);
    Q_ASSERT(ok);
}

WSeat::WSeat(const QString &name)
    : WWrapObject(*new WSeatPrivate(this, name))
{

}

WSeat *WSeat::fromHandle(const QWSeat *handle)
{
    return handle->getData<WSeat>();
}

QWSeat *WSeat::handle() const
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
        Q_FOREACH(auto i, d->deviceList) {
            d->cursor->detachInputDevice(i);
        }

        d->cursor->setSeat(nullptr);
    }

    d->cursor = cursor;

    if (isValid() && cursor) {
        cursor->setSeat(this);

        Q_FOREACH(auto i, d->deviceList) {
            cursor->attachInputDevice(i);
        }
    }
}

WCursor *WSeat::cursor() const
{
    W_DC(WSeat);
    return d->cursor;
}

void WSeat::attachInputDevice(WInputDevice *device)
{
    Q_ASSERT(!device->seat());
    W_D(WSeat);

    d->deviceList << device;

    if (isValid()) {
        if (d->cursor)
            d->cursor->attachInputDevice(device);

        d->attachInputDevice(device);
        d->updateCapabilities();
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

    if (device->type() == WInputDevice::Type::Touch) {
        qCDebug(qLcWlrTouch, "WSeat: detachTouchDevice %s", qPrintable(device->qtDevice()->name()));
        auto *state = device->getAttachedData<WSeatPrivate::DeviceState>();
        device->removeAttachedData<WSeatPrivate::DeviceState>();
        delete state;
        d->touchDeviceList.removeOne(device);
    }
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
        d->doNotifyButton(WCursor::toNativeButton(e->button()), WLR_BUTTON_PRESSED, event->timestamp());
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto e = static_cast<QMouseEvent*>(event);
        Q_ASSERT(e->source() == Qt::MouseEventNotSynthesized);
        d->doNotifyButton(WCursor::toNativeButton(e->button()), WLR_BUTTON_RELEASED, event->timestamp());
        break;
    }
    case QEvent::HoverMove: Q_FALLTHROUGH();
    case QEvent::MouseMove: {
        auto e = static_cast<QMouseEvent*>(event);
        Q_ASSERT(e->source() == Qt::MouseEventNotSynthesized);
        d->doNotifyMotion(target, eventObject, e->position(), e->timestamp());
        break;
    }
    case QEvent::Wheel: {
        if (auto we = dynamic_cast<WSeatWheelEvent*>(event)) {
            Qt::Orientation orientation = we->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
            d->doNotifyAxis(static_cast<wlr_axis_source>(we->wlrSource()),
                        orientation,
                        we->wlrDelta(),
                        we->angleDelta().x()+we->angleDelta().y(), // one of them must be 0
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
        for (const QEventPoint &touchPoint : e->points()) {
            d->doNotifyFullTouchEvent(target, touchPoint.id(), touchPoint.position(), touchPoint.state(), e->timestamp());
        }
        break;
    }
    case QEvent::TouchCancel: {
        d->doTouchNotifyCancel(target->handle());
        break;
    }
    case QEvent::NativeGesture: {
        if (!d->gesture)
            break;
        auto e = static_cast<WGestureEvent*>(event);
        switch (e->gestureType()) {
            case Qt::NativeGestureType::BeginNativeGesture:
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                    d->gesture->sendSwipeBegin(d->handle(), e->timestamp(), e->fingerCount());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::PinchGesture)
                    d->gesture->sendPinchBegin(d->handle(), e->timestamp(), e->fingerCount());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                    d->gesture->sendHoldBegin(d->handle(), e->timestamp(), e->fingerCount());
                break;
            case Qt::NativeGestureType::PanNativeGesture:
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                    d->gesture->sendSwipeUpdate(d->handle(), e->timestamp(), e->delta());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::PinchGesture)
                    d->gesture->sendPinchUpdate(d->handle(), e->timestamp(), e->delta(), d->lastScale, 0);
                break;
            case Qt::NativeGestureType::ZoomNativeGesture:
                d->gesture->sendPinchUpdate(d->handle(), e->timestamp(), e->delta(), d->lastScale, 0);
                break;
            case Qt::NativeGestureType::RotateNativeGesture:
                d->gesture->sendPinchUpdate(d->handle(), e->timestamp(), e->delta(), d->lastScale, e->value());
                break;
            case Qt::NativeGestureType::EndNativeGesture:
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                    d->gesture->sendSwipeEnd(d->handle(), e->timestamp(), e->cancelled());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::PinchGesture)
                    d->gesture->sendPinchEnd(d->handle(), e->timestamp(), e->cancelled());
                if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                    d->gesture->sendHoldEnd(d->handle(), e->timestamp(), e->cancelled());
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
        return WSurface::fromHandle(QWSurface::from(fs));
    return nullptr;
}

void WSeat::setKeyboardFocusTarget(QWSurface *nativeSurface)
{
    W_D(WSeat);
    d->doSetKeyboardFocus(nativeSurface);
}

void WSeat::setKeyboardFocusTarget(WSurface *surface)
{
    W_D(WSeat);

    setKeyboardFocusTarget(surface ? surface->handle() : nullptr);
}

WSurface *WSeat::keyboardFocusSurface() const
{
    W_DC(WSeat);
    if (auto fs = d->keyboardFocusSurface())
        return WSurface::fromHandle(QWSurface::from(fs));
    return nullptr;
}

void WSeat::clearKeyboardFocusSurface()
{
    W_D(WSeat);
    d->doSetKeyboardFocus(nullptr);
}

void WSeat::setKeyboardFocusTarget(QWindow *window)
{
    W_D(WSeat);
    d->focusWindow = window;
}

QWindow *WSeat::focusWindow() const
{
    W_DC(WSeat);
    return d->focusWindow;
}

void WSeat::clearkeyboardFocusWindow()
{
    W_D(WSeat);
    d->focusWindow = nullptr;
}

WInputDevice *WSeat::keyboard() const
{
    W_DC(WSeat);
    auto qwKeyboard = d->handle()->getKeyboard();
    if (qwKeyboard) {
        auto device = WInputDevice::fromHandle(qwKeyboard);
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
    d->handle()->setKeyboard(qobject_cast<QWKeyboard *>(newKeyboard->handle()));
    Q_EMIT this->keyboardChanged();
}

void WSeat::notifyMotion(WCursor *cursor, WInputDevice *device, uint32_t timestamp)
{
    W_D(WSeat);

    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);
    QWindow *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    QMouseEvent e(QEvent::MouseMove, local, global, Qt::NoButton,
                  cursor->state(), d->keyModifiers, qwDevice);
    Q_ASSERT(e.isUpdateEvent());
    e.setTimestamp(timestamp);

    if (w)
        QCoreApplication::sendEvent(w, &e);
}

void WSeat::notifyButton(WCursor *cursor, WInputDevice *device, Qt::MouseButton button,
                         wlr_button_state_t state, uint32_t timestamp)
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

void WSeat::notifyAxis(WCursor *cursor, WInputDevice *device, wlr_axis_source_t source,
                       Qt::Orientation orientation,
                       double delta, int32_t delta_discrete, uint32_t timestamp)
{
    W_D(WSeat);

    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);

    QWindow *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    QPoint angleDelta = orientation == Qt::Horizontal ? QPoint(delta_discrete, 0) : QPoint(0, delta_discrete);
    WSeatWheelEvent e(source, delta, local, global, QPoint(), angleDelta, Qt::NoButton, d->keyModifiers,
                  Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized, qwDevice);
    e.setTimestamp(timestamp);

    if (w) {
        QCoreApplication::sendEvent(w, &e);
    } else {
        d->doNotifyAxis(static_cast<wlr_axis_source>(source), orientation, delta, delta_discrete, timestamp);
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
        for (auto point : state->m_points) {
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
    qCDebug(qLcWlrTouchEvents) << "Touch cancel for device: " << qwDevice->name()
        << ", discard the following state: " << state->m_points;

    if (cursor->eventWindow()) {
        QWindowSystemInterface::handleTouchCancelEvent(cursor->eventWindow(), qwDevice, d->keyModifiers);
    }
}

void WSeat::notifyTouchFrame(WCursor *cursor)
{
    W_D(WSeat);
    Q_UNUSED(cursor);
    for (auto *device: d->touchDeviceList) {
        d->doNotifyTouchFrame(device);
    }
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
    m_handle = QWSeat::create(server->handle(), d->name.toUtf8().constData());
    initHandle(d->handle());
    d->handle()->setData(this, this);
    d->connect();

    Q_FOREACH(auto i, d->deviceList) {
        d->attachInputDevice(i);

        if (d->cursor)
            d->cursor->attachInputDevice(i);
    }
    if (!qEnvironmentVariableIsSet("WAYLIB_DISABLE_GESTURE"))
        d->gesture = QWPointerGesturesV1::create(server->handle());

    d->updateCapabilities();
}

void WSeat::destroy(WServer *)
{
    W_D(WSeat);

    Q_FOREACH(auto i, d->deviceList) {
        i->setSeat(nullptr);
    }

    d->deviceList.clear();

    // Need not call the DCursor::detachInputDevice on destroy WSeat, so do
    // call the detachCursor at clear the deviceList after.
    if (d->cursor)
        setCursor(nullptr);

    if (m_handle) {
        d->handle()->setData(nullptr, nullptr);
        m_handle = nullptr;
    }
}

wl_global *WSeat::global() const
{
    W_D(const WSeat);
    return d->nativeHandle()->global;
}

bool WSeat::filterEventBeforeDisposeStage(QWindow *targetWindow, QInputEvent *event)
{
    W_D(WSeat);

    d->addEventState(event);

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
                    const auto pos = static_cast<QSinglePointEvent*>(event)->position();
                    QQuickWindowPrivate::get(qw)->deliveryAgentPrivate()->lastMousePosition = pos;
                }
            }

            return true;
        }
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
        if (d->handle()->pointerHasGrab())
            return sendEvent(nullptr, nullptr, nullptr, event);
        break;
    case QEvent::KeyPress: Q_FALLTHROUGH();
    case QEvent::KeyRelease:
        if (d->handle()->keyboardHasGrab())
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
