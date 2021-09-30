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
#include "wseat.h"
#include "wcursor.h"
#include "winputdevice.h"
#include "wsignalconnector.h"
#include "woutput.h"
#include "wsurface.h"
#include "wthreadutils.h"

#include <QQuickWindow>
#include <QCoreApplication>
#include <QDebug>

#include <private/qxkbcommon_p.h>

extern "C" {
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

QEvent::Type WInputEvent::m_type = static_cast<QEvent::Type>(QEvent::registerEventType());

class WSeatPrivate : public WObjectPrivate
{
public:
    WSeatPrivate(WSeat *qq, const QByteArray &name)
        : WObjectPrivate(qq)
        , name(name)
    {

    }

    inline wlr_seat *handle() const {
        return q_func()->nativeInterface<wlr_seat>();
    }

    inline void postEvent(WOutput *output, void *nativeEvent, WInputEvent::Type type,
                          WInputDevice *device, WCursor *cursor = nullptr) const {
        WInputEvent::DataPointer data(new WInputEvent::Data);
        data->seat = const_cast<WSeat*>(q_func());
        data->device = device;
        data->nativeEvent = nativeEvent;
        data->cursor = cursor;
        data->eventType = type;
        WInputEvent *we = new WInputEvent(data);
        output->postInputEvent(we);
    }

    inline bool shouldNotifyPointerEvent(WCursor *cursor) {
        if (!hoverSurface)
            return false;
        auto output = cursor->mappedOutput();
        if (Q_LIKELY(output)) {
            if (Q_UNLIKELY(output->attachedWindow()->mouseGrabberItem()))
                return false;
        }
        return true;
    }

    inline void doNotifyMotion(const WCursor *cursor, WInputDevice *device,
                               uint32_t timestamp) {
        Q_UNUSED(device)
        qreal scale = cursor->mappedOutput()->scale();
        auto pos = cursor->position();
        // Get a valid wlr_surface object by the input region of the client
        auto target_surface = hoverSurface->nativeInputTargetAt<wlr_surface>(scale, pos);
        if (Q_UNLIKELY(!target_surface)) {
            // Because the doNotifyMotion will received the event on before the WQuickItem, so
            // when the mouse move form the window edges to outside of the event area of the
            // window, that can't get a valid target surface object at here. Follow, only when
            // the event spread to the WQuickItem's window can trigger the notifyLeaveSurface.
            return;
        }

        if (Q_LIKELY(handle()->pointer_state.focused_surface == target_surface)) {
            wlr_seat_pointer_notify_motion(handle(), timestamp, pos.x(), pos.y());
        } else { // Should to notify the enter event on there
            wlr_seat_pointer_notify_enter(handle(), target_surface, pos.x(), pos.y());
        }
    }
    inline void doNotifyButton(const WCursor *cursor, WInputDevice *device,
                               uint32_t button, WInputDevice::ButtonState state,
                               uint32_t timestamp) {
        Q_UNUSED(cursor);
        Q_UNUSED(device);
        wlr_seat_pointer_notify_button(handle(), timestamp, button,
                                       static_cast<wlr_button_state>(state));

        if (state == WInputDevice::ButtonState::Pressed
                && Qt::LeftButton == WCursor::fromNativeButton(button)) {
            wlr_seat_keyboard_enter(handle(), handle()->pointer_state.focused_surface, nullptr, 0, nullptr);
        }
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
        wlr_seat_pointer_notify_axis(handle(), timestamp, fromQtHorizontal(orientation), delta,
                                     delta_discrete, static_cast<wlr_axis_source>(source));
    }
    inline void doNotifyFrame(const WCursor *cursor) {
        Q_UNUSED(cursor);
        wlr_seat_pointer_notify_frame(handle());
    }
    inline void doEnter(WSurface *surface, WInputEvent::DataPointer data) {
        hoverSurface = surface;
        qreal scale = data->cursor->mappedOutput()->scale();
        auto pos = data->cursor->position();
        auto target_surface = surface->nativeInputTargetAt<wlr_surface>(scale, pos);
        // When the hoverSuface is exists, indicate the event receive areas is filtered
        // by the WQuickItem, so the target_suface should always is not nullptr.
        Q_ASSERT(target_surface);
        wlr_seat_pointer_notify_enter(handle(), target_surface, pos.x(), pos.y());
    }
    inline void doClearFocus(WInputEvent::DataPointer data) {
        hoverSurface = nullptr;
        wlr_seat_pointer_notify_clear_focus(handle());
        // Should do restore to the Qt window's cursor when the cursor from 
        // the surface of client move out.
        data->cursor->reset();
    }

    // for keyboard event
    inline bool doNotifyServer();
    inline void doNotifyKey(WInputDevice *device, uint32_t keycode,
                            WInputDevice::KeyState state, uint32_t timestamp) {
        auto handle = device->nativeInterface<wlr_input_device>();
        wlr_seat_set_keyboard(this->handle(), handle);
        /* Send modifiers to the client. */
        wlr_seat_keyboard_notify_key(this->handle(), timestamp,
                                     keycode, static_cast<uint32_t>(state));
    }
    inline void doNotifyModifiers(WInputDevice *device) {
        auto handle = device->nativeInterface<wlr_input_device>();
        wlr_seat_set_keyboard(this->handle(), handle);
        /* Send modifiers to the client. */
        wlr_seat_keyboard_notify_modifiers(this->handle(),
                                           &handle->keyboard->modifiers);
    }

    // begin slot function
    void on_destroy(void *data);
    void on_request_set_cursor(void *data);
    void on_request_set_selection(void *data);

    void on_keyboard_key(void *signalData, void *data);
    void on_keyboard_modifiers(void *signalData, void *data);
    // end slot function

    void connect();
    void updateCapabilities();
    void attachInputDevice(WInputDevice *device);

    W_DECLARE_PUBLIC(WSeat)

    QByteArray name;
    WCursor* cursor = nullptr;
    QVector<WInputDevice*> deviceList;
    QPointer<WSurface> hoverSurface;
    WServer *server = nullptr;
    QPointer<QObject> eventGrabber;

    // for event data
    Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;

    WSignalConnector sc;
};

void WSeatPrivate::on_destroy(void *)
{
    q_func()->m_handle = nullptr;
    sc.invalidate();
}

void WSeatPrivate::on_request_set_cursor(void *data)
{
    auto event = reinterpret_cast<wlr_seat_pointer_request_set_cursor_event*>(data);
    auto focused_client = handle()->pointer_state.focused_client;
    /* This can be sent by any client, so we check to make sure this one is
     * actually has pointer focus first. */
    if (focused_client == event->seat_client) {
        /* Once we've vetted the client, we can tell the cursor to use the
         * provided surface as the cursor image. It will set the hardware cursor
         * on the output that it's currently on and continue to do so as the
         * cursor moves between outputs. */

        wlr_cursor_set_surface(cursor->nativeInterface<wlr_cursor>(), event->surface,
                               event->hotspot_x, event->hotspot_y);
    }
}

void WSeatPrivate::on_request_set_selection(void *data)
{
    auto event = reinterpret_cast<wlr_seat_request_set_selection_event*>(data);
    wlr_seat_set_selection(handle(), event->source, event->serial);
}

void WSeatPrivate::on_keyboard_key(void *signalData, void *data)
{
    auto event = static_cast<wlr_event_keyboard_key*>(signalData);
    auto device = static_cast<WInputDevice*>(data);

    q_func()->notifyKey(device, event->keycode,
                        static_cast<WInputDevice::KeyState>(event->state),
                        event->time_msec);
}

void WSeatPrivate::on_keyboard_modifiers(void *signalData, void *data)
{
    Q_UNUSED(signalData)
    auto device = static_cast<WInputDevice*>(data);

    q_func()->notifyModifiers(device);
}

void WSeatPrivate::connect()
{
    sc.connect(&handle()->events.destroy,
               this, &WSeatPrivate::on_destroy);
    sc.connect(&handle()->events.request_set_cursor,
               this, &WSeatPrivate::on_request_set_cursor);
    sc.connect(&handle()->events.request_set_selection,
               this, &WSeatPrivate::on_request_set_selection);
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

    wlr_seat_set_capabilities(handle(), caps);
}

void WSeatPrivate::attachInputDevice(WInputDevice *device)
{
    W_Q(WSeat);
    device->setSeat(q);

    if (device->type() == WInputDevice::Type::Keyboard) {
        auto keyboard = device->deviceNativeInterface<wlr_keyboard>();

        /* We need to prepare an XKB keymap and assign it to the keyboard. This
         * assumes the defaults (e.g. layout = "us"). */
        struct xkb_rule_names rules = {};
        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
                                                           XKB_KEYMAP_COMPILE_NO_FLAGS);

        wlr_keyboard_set_keymap(keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        wlr_keyboard_set_repeat_info(keyboard, 25, 600);

        sc.connect(&keyboard->events.key, this,
                   &WSeatPrivate::on_keyboard_key, device);
        sc.connect(&keyboard->events.modifiers, this,
                   &WSeatPrivate::on_keyboard_modifiers, device);
    }
}

WSeat::WSeat(const QByteArray &name)
    : WObject(*new WSeatPrivate(this, name))
{

}

WSeat *WSeat::fromHandle(const void *handle)
{
    auto wlr_handle = reinterpret_cast<const wlr_seat*>(handle);
    return reinterpret_cast<WSeat*>(wlr_handle->data);
}

void WSeat::attachCursor(WCursor *cursor)
{
    W_D(WSeat);
    Q_ASSERT(!cursor->seat());
    Q_ASSERT(d->cursor == cursor || !d->cursor);
    d->cursor = cursor;

    if (isValid()) {
        cursor->setSeat(this);

        Q_FOREACH(auto i, d->deviceList) {
            cursor->attachInputDevice(i);
        }
    }
}

void WSeat::detachCursor(WCursor *cursor)
{
    W_D(WSeat);
    Q_ASSERT(d->cursor == cursor);
    cursor->setSeat(nullptr);
    d->cursor = nullptr;

    Q_FOREACH(auto i, d->deviceList) {
        cursor->detachInputDevice(i);
    }
}

WCursor *WSeat::attachedCursor() const
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
}

void WSeat::notifyEnterSurface(WSurface *surface, WInputEvent *event)
{
    W_D(WSeat);
    // Do async call that the event maybe to destroyed later, so should
    // use the data of event instead of the event self.
    d->server->threadUtil()->run(surface, d, &WSeatPrivate::doEnter,
                                 surface, event->data);
}

void WSeat::notifyLeaveSurface(WSurface *surface, WInputEvent *event)
{
    Q_UNUSED(event)
    W_D(WSeat);
    Q_ASSERT(d->hoverSurface == surface);
    // Do async call that the event maybe to destroyed later, so should
    // use the data of event instead of the event self.
    d->server->threadUtil()->run(d->server, d, &WSeatPrivate::doClearFocus, event->data);
}

WSurface *WSeat::hoverSurface() const
{
    W_DC(WSeat);
    return d->hoverSurface;
}

void WSeat::notifyMotion(WCursor *cursor, WInputDevice *device,
                          uint32_t timestamp)
{
    W_D(WSeat);

    const QPointF &global = cursor->position();

    if (Q_UNLIKELY(d->eventGrabber)) {
        QMouseEvent e(QEvent::MouseMove, global, cursor->button(),
                      cursor->state(), d->keyModifiers);
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

    auto output = cursor->mappedOutput();
    if (Q_UNLIKELY(!output))
        return;

    QWindow *w = output->attachedWindow();

    if (Q_UNLIKELY(!w))
        return;

    const QPointF &local = global - QPointF(w->position());

    auto e = new QMouseEvent(QEvent::MouseMove, local, local, global,
                             cursor->button(), cursor->state(), d->keyModifiers);
    e->setTimestamp(timestamp);
    d->postEvent(cursor->mappedOutput(), e, WInputEvent::PointerMotion, device, cursor);
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
                      cursor->state(), d->keyModifiers);
        e.setTimestamp(timestamp);
        QCoreApplication::sendEvent(d->eventGrabber.data(), &e);

        if (e.isAccepted())
            return;
    }

    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        return d->doNotifyButton(cursor, device, button, state, timestamp);
    }

    auto output = cursor->mappedOutput();
    if (Q_UNLIKELY(!output))
        return;

    QWindow *w = output->attachedWindow();

    if (Q_UNLIKELY(!w))
        return;

    const QPointF &local = global - QPointF(w->position());

    auto e = new QMouseEvent(et, local, local, global, cursor->button(),
                             cursor->state(), d->keyModifiers);
    e->setTimestamp(timestamp);
    d->postEvent(cursor->mappedOutput(), e, WInputEvent::PointerButton, device, cursor);
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
        return d->doNotifyFrame(cursor);
    }
}

void WSeat::notifyKey(WInputDevice *device, uint32_t keycode,
                       WInputDevice::KeyState state, uint32_t timestamp)
{
    W_D(WSeat);

    auto handle = device->nativeInterface<wlr_input_device>();
    auto code = keycode + 8; // map to wl_keyboard::keymap_format::keymap_format_xkb_v1
    auto et = state == WInputDevice::KeyState::Pressed ? QEvent::KeyPress : QEvent::KeyRelease;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(handle->keyboard->xkb_state, code);
    int qtkey = QXkbCommon::keysymToQtKey(sym, d->keyModifiers, handle->keyboard->xkb_state, code);
    const QString &text = QXkbCommon::lookupString(handle->keyboard->xkb_state, code);
    QKeyEvent e(et, qtkey, d->keyModifiers, keycode, code,
                wlr_keyboard_get_modifiers(handle->keyboard), text);
    WInputEvent::DataPointer data(new WInputEvent::Data);
    data->seat = this;
    data->device = device;
    data->nativeEvent = &e;
    data->eventType = WInputEvent::KeyboardKey;
    WInputEvent we(data);
    QCoreApplication::sendEvent(d->server, &we);

    if (!we.isAccepted())
        d->doNotifyKey(device, keycode, state, timestamp);
}

void WSeat::notifyModifiers(WInputDevice *device)
{
    auto handle = device->nativeInterface<wlr_input_device>();

    W_D(WSeat);
    d->keyModifiers = QXkbCommon::modifiers(handle->keyboard->xkb_state);
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
    m_handle = wlr_seat_create(server->nativeInterface<wl_display>(),
                               d->name.constData());
    d->server = server;
    d->handle()->data = this;
    d->connect();

    if (d->cursor)
        attachCursor(d->cursor);

    Q_FOREACH(auto i, d->deviceList) {
        d->attachInputDevice(i);
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
        detachCursor(d->cursor);

    if (m_handle) {
        d->handle()->data = nullptr;
        wlr_seat_destroy(d->handle());
        m_handle = nullptr;
    }

    d->server = nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
