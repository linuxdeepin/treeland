// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wseat.h"
#include "wcursor.h"
#include "winputdevice.h"
#include "wsignalconnector.h"
#include "woutput.h"
#include "wsurface.h"
#include "platformplugin/qwlrootsintegration.h"

#include <qwseat.h>
#include <qwkeyboard.h>
#include <qwcursor.h>
#include <qwcompositor.h>

#include <QQuickWindow>
#include <QGuiApplication>
#include <QQuickItem>
#include <QDebug>

#include <private/qxkbcommon_p.h>

extern "C" {
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#define static
#include <wlr/types/wlr_cursor.h>
#undef static
#include <wlr/types/wlr_xdg_shell.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

//QEvent::Type WInputEvent::m_type = static_cast<QEvent::Type>(QEvent::registerEventType());

class WSeatPrivate : public WObjectPrivate
{
public:
    WSeatPrivate(WSeat *qq, const QByteArray &name)
        : WObjectPrivate(qq)
        , name(name)
    {

    }

    inline QWSeat *handle() const {
        return q_func()->nativeInterface<QWSeat>();
    }

    inline bool shouldNotifyPointerEvent(WCursor *cursor) {
        if (!hoverSurface)
            return false;
        auto w = cursor->eventWindow();
        if (Q_LIKELY(w)) {
            auto item = w->mouseGrabberItem();
            if (Q_UNLIKELY(item && item->keepMouseGrab()))
                return false;
        }
        return true;
    }

    inline void doNotifyMotion(const WCursor *cursor, WInputDevice *device,
                               uint32_t timestamp) {
        Q_UNUSED(device)
        auto pos = hoverSurface->mapFromGlobal(cursor->position());
        // Get a valid wlr_surface object by the input region of the client
        auto target_surface = hoverSurface->nativeInputTargetAt<QWSurface>(pos)->handle();
        if (Q_UNLIKELY(!target_surface)) {
            // Because the doNotifyMotion will received the event on before the WQuickItem, so
            // when the mouse move form the window edges to outside of the event area of the
            // window, that can't get a valid target surface object at here. Follow, only when
            // the event spread to the WQuickItem's window can trigger the notifyLeaveSurface.
            return;
        }

        if (Q_LIKELY(handle()->handle()->pointer_state.focused_surface == target_surface)) {
            wlr_seat_pointer_notify_motion(handle()->handle(), timestamp, pos.x(), pos.y());
        } else { // Should to notify the enter event on there
            wlr_seat_pointer_notify_enter(handle()->handle(), target_surface, pos.x(), pos.y());
        }
    }
    inline void doNotifyButton(const WCursor *cursor, WInputDevice *device,
                               uint32_t button, WInputDevice::ButtonState state,
                               uint32_t timestamp) {
        Q_UNUSED(cursor);
        Q_UNUSED(device);
        wlr_seat_pointer_notify_button(handle()->handle(), timestamp, button,
                                       static_cast<wlr_button_state>(state));
    }
    static inline wlr_axis_orientation fromQtHorizontal(Qt::Orientation o) {
        return o == Qt::Horizontal ? WLR_AXIS_ORIENTATION_HORIZONTAL
                                   : WLR_AXIS_ORIENTATION_VERTICAL;
    }
    inline void doNotifyAxis(const WCursor *cursor, WInputDevice *device,
                             WInputDevice::AxisSource source, Qt::Orientation orientation,
                             double delta, int32_t delta_discrete, uint32_t timestamp) {
        Q_UNUSED(cursor)
        Q_UNUSED(device)
        wlr_seat_pointer_notify_axis(handle()->handle(), timestamp, fromQtHorizontal(orientation), delta,
                                     delta_discrete, static_cast<wlr_axis_source>(source));
    }
    inline void doNotifyFrame(const WCursor *cursor) {
        Q_UNUSED(cursor);
        wlr_seat_pointer_notify_frame(handle()->handle());
    }
    inline void doEnter(WSurface *surface) {
        hoverSurface = surface;
        auto pos = surface->mapFromGlobal(cursor->position());
        auto target_surface = surface->nativeInputTargetAt<QWSurface>(pos)->handle();
        // When the hoverSuface is exists, indicate the event receive areas is filtered
        // by the WQuickItem, so the target_suface should always is not nullptr.
        Q_ASSERT(target_surface);
        wlr_seat_pointer_notify_enter(handle()->handle(), target_surface, pos.x(), pos.y());
    }
    inline void doClearFocus() {
        hoverSurface = nullptr;
        wlr_seat_pointer_notify_clear_focus(handle()->handle());
    }
    inline void doSetKeyboardFocus(wlr_surface *surface) {
        if (surface) {
            wlr_seat_keyboard_enter(handle()->handle(), surface, nullptr, 0, nullptr);
        } else {
            wlr_seat_keyboard_clear_focus(handle()->handle());
        }
    }

    // for keyboard event
    inline bool doNotifyServer();
    inline void doNotifyKey(WInputDevice *device, uint32_t keycode,
                            WInputDevice::KeyState state, uint32_t timestamp) {
        auto handle = device->nativeInterface<QWInputDevice>();
        this->handle()->setKeyboard(qobject_cast<QWKeyboard*>(handle));
        /* Send modifiers to the client. */
        this->handle()->keyboardNotifyKey(timestamp, keycode, static_cast<uint32_t>(state));
    }
    inline void doNotifyModifiers(WInputDevice *device) {
        auto handle = device->nativeInterface<QWInputDevice>();
        auto keyboard = qobject_cast<QWKeyboard*>(handle);
        this->handle()->setKeyboard(keyboard);
        /* Send modifiers to the client. */
        this->handle()->keyboardNotifyModifiers(&keyboard->handle()->modifiers);
    }

    // begin slot function
    void on_destroy();
    void on_request_set_cursor(wlr_seat_pointer_request_set_cursor_event *event);
    void on_request_set_selection(wlr_seat_request_set_selection_event *event);

    void on_keyboard_key(wlr_keyboard_key_event *event, WInputDevice *device);
    void on_keyboard_modifiers(WInputDevice *device);
    // end slot function

    void connect();
    void updateCapabilities();
    void attachInputDevice(WInputDevice *device);

    W_DECLARE_PUBLIC(WSeat)

    QByteArray name;
    WCursor* cursor = nullptr;
    QVector<WInputDevice*> deviceList;
    QPointer<WSurface> hoverSurface;
    QPointer<QObject> eventGrabber;

    // for event data
    Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;
};

void WSeatPrivate::on_destroy()
{
    q_func()->m_handle = nullptr;
}

void WSeatPrivate::on_request_set_cursor(wlr_seat_pointer_request_set_cursor_event *event)
{
    auto focused_client = handle()->handle()->pointer_state.focused_client;
    /* This can be sent by any client, so we check to make sure this one is
     * actually has pointer focus first. */
    if (focused_client == event->seat_client) {
        /* Once we've vetted the client, we can tell the cursor to use the
         * provided surface as the cursor image. It will set the hardware cursor
         * on the output that it's currently on and continue to do so as the
         * cursor moves between outputs. */
        cursor->handle()->setSurface(QWSurface::from(event->surface), QPoint(event->hotspot_x, event->hotspot_y));
    }
}

void WSeatPrivate::on_request_set_selection(wlr_seat_request_set_selection_event *event)
{
    handle()->setSelection(event->source, event->serial);
}

void WSeatPrivate::on_keyboard_key(wlr_keyboard_key_event *event, WInputDevice *device)
{
    q_func()->notifyKey(device, event->keycode,
                        static_cast<WInputDevice::KeyState>(event->state),
                        event->time_msec);
}

void WSeatPrivate::on_keyboard_modifiers(WInputDevice *device)
{
    q_func()->notifyModifiers(device);
}

void WSeatPrivate::connect()
{
    QObject::connect(handle(), &QWSeat::destroyed, q_func()->server(), [this] {
        on_destroy();
    });
    QObject::connect(handle(), &QWSeat::requestSetCursor, q_func()->server()->slotOwner(), [this] (wlr_seat_pointer_request_set_cursor_event *event) {
        on_request_set_cursor(event);
    });
    QObject::connect(handle(), &QWSeat::requestSetSelection, q_func()->server()->slotOwner(), [this] (wlr_seat_request_set_selection_event *event) {
        on_request_set_selection(event);
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
    QWlrootsIntegration::instance()->addInputDevice(device);

    if (device->type() == WInputDevice::Type::Keyboard) {
        auto keyboard = qobject_cast<QWKeyboard*>(device->nativeInterface<QWInputDevice>());

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

        QObject::connect(keyboard, &QWKeyboard::key, q_func()->server()->slotOwner(), [this, device] (wlr_keyboard_key_event *event) {
            on_keyboard_key(event, device);
        });
        QObject::connect(keyboard, &QWKeyboard::modifiers, q_func()->server()->slotOwner(), [this, device] () {
            on_keyboard_modifiers(device);
        });
    }
}

WSeat::WSeat(const QByteArray &name)
    : WObject(*new WSeatPrivate(this, name))
{

}

WSeat *WSeat::fromHandle(const void *handle)
{
    return reinterpret_cast<const QWSeat*>(handle)->getData<WSeat>();
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

    if (!isValid()) {
        return;
    }

    if (d->cursor)
        d->cursor->attachInputDevice(device);

    d->attachInputDevice(device);
    d->updateCapabilities();
}

void WSeat::detachInputDevice(WInputDevice *device)
{
    W_D(WSeat);
    device->setSeat(nullptr);
    d->deviceList.removeOne(device);

    if (!isValid()) {
        return;
    }

    d->updateCapabilities();

    if (d->cursor && device->type() == WInputDevice::Type::Pointer) {
        d->cursor->detachInputDevice(device);
    }

    QWlrootsIntegration::instance()->removeInputDevice(device);
}

void WSeat::notifyEnterSurface(WSurface *surface)
{
    W_D(WSeat);
    d->doEnter(surface);
}

void WSeat::notifyLeaveSurface(WSurface *surface)
{
    W_D(WSeat);
    Q_ASSERT(d->hoverSurface == surface);
    d->doClearFocus();
}

WSurface *WSeat::hoverSurface() const
{
    W_DC(WSeat);
    return d->hoverSurface;
}

void WSeat::setKeyboardFocusTarget(WSurfaceHandle *nativeSurface)
{
    W_D(WSeat);
    d->doSetKeyboardFocus(reinterpret_cast<QWSurface*>(nativeSurface)->handle());
}

void WSeat::setKeyboardFocusTarget(WSurface *surface)
{
    W_D(WSeat);

    if (surface) {
        setKeyboardFocusTarget(surface->handle());
    } else {
        setKeyboardFocusTarget(static_cast<WSurfaceHandle*>(nullptr));
    }
}

void WSeat::notifyMotion(WCursor *cursor, WInputDevice *device,
                         uint32_t timestamp)
{
    W_D(WSeat);

    const QPointF &global = cursor->position();

    if (Q_UNLIKELY(d->eventGrabber)) {
        QMouseEvent e(QEvent::MouseMove, global, cursor->button(),
                      cursor->state(), d->keyModifiers, device->qtDevice<QPointingDevice>());
        e.setTimestamp(timestamp);
        QCoreApplication::sendEvent(d->eventGrabber.data(), &e);

        if (e.isAccepted())
            return;
    }

    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        d->doNotifyMotion(cursor, device, timestamp);
        // Continue to spread the event, in order to the QQuickWindow do handle
        // the enter and leave events.
    }

    QWindow *w = cursor->eventWindow();
    if (Q_UNLIKELY(!w))
        return;

    const QPointF &local = global - QPointF(w->position());
    auto e = new QMouseEvent(QEvent::MouseMove, local, local, global,
                             cursor->button(), cursor->state(), d->keyModifiers,
                             device->qtDevice<QPointingDevice>());
    e->setTimestamp(timestamp);
    QCoreApplication::sendEvent(w, e);
}

void WSeat::notifyButton(WCursor *cursor, WInputDevice *device, uint32_t button,
                         WInputDevice::ButtonState state, uint32_t timestamp)
{
    W_D(WSeat);

    const QPointF &global = cursor->position();
    const auto et = state == WInputDevice::ButtonState::Pressed ? QEvent::MouseButtonPress
                                                                : QEvent::MouseButtonRelease;

    if (Q_UNLIKELY(d->eventGrabber)) {
        QMouseEvent e(et, global, cursor->button(),
                      cursor->state(), d->keyModifiers, device->qtDevice<QPointingDevice>());
        e.setTimestamp(timestamp);
        QCoreApplication::sendEvent(d->eventGrabber.data(), &e);

        if (e.isAccepted())
            return;
    }

    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        d->doNotifyButton(cursor, device, button, state, timestamp);
    }

    QWindow *w = cursor->eventWindow();
    if (Q_UNLIKELY(!w))
        return;

    const QPointF &local = global - QPointF(w->position());
    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);
    auto e = new QMouseEvent(et, local, local, global, cursor->button(),
                             cursor->state(), d->keyModifiers, qwDevice);
    e->setTimestamp(timestamp);
    QCoreApplication::sendEvent(w, e);
}

void WSeat::notifyAxis(WCursor *cursor, WInputDevice *device,
                        WInputDevice::AxisSource source, Qt::Orientation orientation,
                        double delta, int32_t delta_discrete, uint32_t timestamp)
{
    W_D(WSeat);
    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        return d->doNotifyAxis(cursor, device, source, orientation,
                               delta, delta_discrete, timestamp);
    }
}

void WSeat::notifyFrame(WCursor *cursor)
{
    W_D(WSeat);
    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        d->doNotifyFrame(cursor);
    }
}

void WSeat::notifyKey(WInputDevice *device, uint32_t keycode,
                      WInputDevice::KeyState state, uint32_t timestamp)
{
    W_D(WSeat);

    auto handle = device->nativeInterface<QWInputDevice>();
    auto keyboard = qobject_cast<QWKeyboard*>(handle);
    auto code = keycode + 8; // map to wl_keyboard::keymap_format::keymap_format_xkb_v1
    auto et = state == WInputDevice::KeyState::Pressed ? QEvent::KeyPress : QEvent::KeyRelease;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(keyboard->handle()->xkb_state, code);
    int qtkey = QXkbCommon::keysymToQtKey(sym, d->keyModifiers, keyboard->handle()->xkb_state, code);
    const QString &text = QXkbCommon::lookupString(keyboard->handle()->xkb_state, code);

    // TODO: Use the focus window
    auto w = d->cursor->eventWindow();
    if (w) {
        QKeyEvent e(et, qtkey, d->keyModifiers, keycode, code, keyboard->getModifiers(),
                    text, false, 1, device->qtDevice());
        QCoreApplication::sendEvent(w, &e);

        if (!e.isAccepted())
            d->doNotifyKey(device, keycode, state, timestamp);
    } else {
        d->doNotifyKey(device, keycode, state, timestamp);
    }
}

void WSeat::notifyModifiers(WInputDevice *device)
{
    auto handle = device->nativeInterface<QWInputDevice>();
    auto keyboard = qobject_cast<QWKeyboard*>(handle);

    W_D(WSeat);
    d->keyModifiers = QXkbCommon::modifiers(keyboard->handle()->xkb_state);
    d->doNotifyModifiers(device);
}

QObject *WSeat::eventGrabber() const
{
    W_DC(WSeat);
    return d->eventGrabber.data();
}

void WSeat::setEventGrabber(QObject *grabber)
{
    W_D(WSeat);
    Q_ASSERT(!grabber || !d->eventGrabber);
    d->eventGrabber = grabber;
}

void WSeat::create(WServer *server)
{
    W_D(WSeat);
    // destroy follow display
    m_handle = QWSeat::create(server->nativeInterface<QWDisplay>(), d->name.constData());
    d->handle()->setData(this, this);
    d->connect();

    Q_FOREACH(auto i, d->deviceList) {
        d->attachInputDevice(i);

        if (d->cursor)
            d->cursor->attachInputDevice(i);
    }

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
        d->handle()->setData(this, nullptr);
        m_handle = nullptr;
    }
}

WAYLIB_SERVER_END_NAMESPACE
