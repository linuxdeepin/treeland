// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wseat.h"
#include "wcursor.h"
#include "winputdevice.h"
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

class WSeatPrivate : public WObjectPrivate
{
public:
    WSeatPrivate(WSeat *qq, const QString &name)
        : WObjectPrivate(qq)
        , name(name)
    {

    }

    inline QWSeat *handle() const {
        return q_func()->nativeInterface<QWSeat>();
    }

    inline wlr_seat *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    inline bool shouldNotifyPointerEvent(WCursor *cursor) {
        if (!pointerEventGrabber)
            return false;
        auto w = cursor->eventWindow();
        if (Q_LIKELY(w)) {
            auto item = w->mouseGrabberItem();
            if (Q_UNLIKELY(item && item->keepMouseGrab()))
                return false;
        }
        return true;
    }

    inline wlr_surface *pointerFocusSurface() const {
        return nativeHandle()->pointer_state.focused_surface;
    }

    inline wlr_surface *keyboardFocusSurface() const {
        return nativeHandle()->keyboard_state.focused_surface;
    }

    inline bool doNotifyMotion(WSurface *target, QPointF localPos, uint32_t timestamp) {
        // Get a valid wlr_surface object by the input region of the client
        auto target_surface = target->inputTargetAt(localPos)->handle();
        if (Q_UNLIKELY(!target_surface)) {
            // Because the doNotifyMotion will received the event on before the QQuickItem, so
            // when the mouse move form the window edges to outside of the event area of the
            // window, that can't get a valid target surface object at here. Follow, only when
            // the event spread to the QQuickItem's window can trigger the notifyLeaveSurface.
            return false;
        }

        if (Q_LIKELY(pointerFocusSurface() == target_surface)) {
            handle()->pointerNotifyMotion(timestamp, localPos.x(), localPos.y());
        } else { // Should to notify the enter event on there
            handle()->pointerNotifyEnter(QWSurface::from(target_surface), localPos.x(), localPos.y());
        }

        return true;
    }
    inline bool doNotifyButton(uint32_t button, wlr_button_state state, uint32_t timestamp) {
        if (!pointerFocusSurface())
            return false;

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
    inline void doEnter(WSurface *surface) {
        auto pos = surface->mapFromGlobal(cursor->position());
        auto target_surface = surface->inputTargetAt(pos)->handle();
        // When the hoverSuface is exists, indicate the event receive areas is filtered
        // by the WQuickItem, so the target_suface should always is not nullptr.
        Q_ASSERT(target_surface);
        handle()->pointerNotifyEnter(QWSurface::from(target_surface), pos.x(), pos.y());
    }
    inline void doClearPointerFocus() {
        handle()->pointerNotifyClearFocus();
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

    // for keyboard event
    inline bool doNotifyKey(WInputDevice *device, uint32_t keycode, uint32_t state, uint32_t timestamp) {
        if (!keyboardFocusSurface())
            return false;

        this->handle()->setKeyboard(qobject_cast<QWKeyboard*>(device->handle()));
        /* Send modifiers to the client. */
        this->handle()->keyboardNotifyKey(timestamp, keycode, state);
        return true;
    }
    inline bool doNotifyModifiers(WInputDevice *device) {
        if (!keyboardFocusSurface())
            return false;

        auto keyboard = qobject_cast<QWKeyboard*>(device->handle());
        this->handle()->setKeyboard(keyboard);
        /* Send modifiers to the client. */
        this->handle()->keyboardNotifyModifiers(&keyboard->handle()->modifiers);
        return true;
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

    QString name;
    WCursor* cursor = nullptr;
    QVector<WInputDevice*> deviceList;
    QPointer<WSurface> pointerEventGrabber;
    QPointer<WSeatEventFilter> eventFilter;
    QPointer<QWindow> focusWindow;

    // for event data
    Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;
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
        cursor->handle()->setSurface(surface, QPoint(event->hotspot_x, event->hotspot_y));
    }
}

void WSeatPrivate::on_request_set_selection(wlr_seat_request_set_selection_event *event)
{
    handle()->setSelection(event->source, event->serial);
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

    if (eventFilter && eventFilter->eventFilter(q_func(), focusWindow, &e))
        return;

    if (focusWindow) {
        QCoreApplication::sendEvent(focusWindow, &e);

        if (!e.isAccepted() && eventFilter)
            eventFilter->ignoredEventFilter(q_func(), focusWindow, &e);
    } else {
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
    QWlrootsIntegration::instance()->addInputDevice(device, name);

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

        QObject::connect(keyboard, &QWKeyboard::key, q_func()->server()->slotOwner(), [this, device] (wlr_keyboard_key_event *event) {
            on_keyboard_key(event, device);
        });
        QObject::connect(keyboard, &QWKeyboard::modifiers, q_func()->server()->slotOwner(), [this, device] () {
            on_keyboard_modifiers(device);
        });
    }
}

WSeat::WSeat(const QString &name)
    : WObject(*new WSeatPrivate(this, name))
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

inline static WSeat *getSeat(QInputEvent *event)
{
    auto inputDevice = WInputDevice::from(event->device());
    if (Q_UNLIKELY(!inputDevice))
        return nullptr;

    return inputDevice->seat();
}

bool WSeat::sendEvent(WSurface *target, QInputEvent *event)
{
    if (event->timestamp() == 0)
        return false;

    auto inputDevice = WInputDevice::from(event->device());
    if (Q_UNLIKELY(!inputDevice))
        return false;

    auto seat = inputDevice->seat();
    auto d = seat->d_func();

    if (d->eventFilter && d->eventFilter->eventFilter(seat, target, event))
        return true;

    event->accept();

    switch (event->type()) {
    case QEvent::HoverEnter:
        d->doEnter(target);
        break;
    case QEvent::HoverLeave:
        Q_ASSERT(d->nativeHandle()->pointer_state.focused_surface);
        d->doClearPointerFocus();
        break;
    case QEvent::MouseButtonPress: {
        auto e = static_cast<QSinglePointEvent*>(event);
        d->doNotifyButton(WCursor::toNativeButton(e->button()), WLR_BUTTON_PRESSED, event->timestamp());
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto e = static_cast<QSinglePointEvent*>(event);
        d->doNotifyButton(WCursor::toNativeButton(e->button()), WLR_BUTTON_RELEASED, event->timestamp());
        break;
    }
    case QEvent::HoverMove: Q_FALLTHROUGH();
    case QEvent::MouseMove: {
        auto e = static_cast<QSinglePointEvent*>(event);
        d->doNotifyMotion(target, e->position(), e->timestamp());
        break;
    }
    case QEvent::KeyPress: {
        auto e = static_cast<QKeyEvent*>(event);
        d->doNotifyKey(inputDevice, e->nativeVirtualKey(), WL_KEYBOARD_KEY_STATE_PRESSED, e->timestamp());
        break;
    }
    case QEvent::KeyRelease: {
        auto e = static_cast<QKeyEvent*>(event);
        d->doNotifyKey(inputDevice, e->nativeVirtualKey(), WL_KEYBOARD_KEY_STATE_RELEASED, e->timestamp());
        break;
    }
    default:
        event->ignore();
        return false;
    }

    return true;
}

void WSeat::setPointerEventGrabber(WSurface *surface)
{
    W_D(WSeat);
    d->pointerEventGrabber = surface;
}

WSurface *WSeat::pointerFocusSurface() const
{
    W_DC(WSeat);
    if (auto fs = d->pointerFocusSurface())
        return WSurface::fromHandle(QWSurface::from(fs));
    return nullptr;
}

WSurface *WSeat::pointerEventGrabber() const
{
    W_DC(WSeat);
    return d->pointerEventGrabber;
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

void WSeat::notifyMotion(WCursor *cursor, WInputDevice *device, uint32_t timestamp)
{
    W_D(WSeat);

    auto qwDevice = static_cast<QPointingDevice*>(device->qtDevice());
    Q_ASSERT(qwDevice);
    QWindow *w = cursor->eventWindow();
    const QPointF &global = cursor->position();
    const QPointF local = w ? global - QPointF(w->position()) : QPointF();

    QMouseEvent e(QEvent::MouseMove, local, global, cursor->button(),
                  cursor->state(), d->keyModifiers, qwDevice);
    e.setTimestamp(timestamp);

    if (Q_UNLIKELY(d->eventFilter)) {
        if (d->eventFilter->eventFilter(this, w, &e))
            return;
    }

    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        const QPointF local = d->pointerEventGrabber->mapFromGlobal(cursor->position());
        d->doNotifyMotion(d->pointerEventGrabber, local, timestamp);
        return;
    }

    if (w)
        QCoreApplication::sendEvent(w, &e);

    if (!e.isAccepted() && d->eventFilter)
        d->eventFilter->ignoredEventFilter(this, w, &e);
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

    QMouseEvent e(et, local, global, cursor->button(),
                  cursor->state(), d->keyModifiers, qwDevice);
    e.setTimestamp(timestamp);

    if (Q_UNLIKELY(d->eventFilter)) {
        if (d->eventFilter->eventFilter(this, w, &e))
            return;
    }

    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        d->doNotifyButton(WCursor::toNativeButton(button),
                          static_cast<wlr_button_state>(state), timestamp);
        return;
    }

    if (w)
        QCoreApplication::sendEvent(w, &e);

    if (!e.isAccepted() && d->eventFilter)
        d->eventFilter->ignoredEventFilter(this, w, &e);
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
    QWheelEvent e(local, global, QPoint(), angleDelta, Qt::NoButton, d->keyModifiers,
                  Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized, qwDevice);
    e.setTimestamp(timestamp);

    if (Q_UNLIKELY(d->eventFilter)) {
        if (d->eventFilter->eventFilter(this, w, &e))
            return;
    }

    if (Q_LIKELY(d->shouldNotifyPointerEvent(cursor))) {
        d->doNotifyAxis(static_cast<wlr_axis_source>(source), orientation, delta, delta_discrete, timestamp);
        return;
    }

    if (w)
        QCoreApplication::sendEvent(w, &e);

    if (e.isAccepted())
        return;

    if (d->doNotifyAxis(static_cast<wlr_axis_source>(source), orientation, delta, delta_discrete, timestamp))
        return;

    if (d->eventFilter)
        d->eventFilter->ignoredEventFilter(this, w, &e);
}

void WSeat::notifyFrame(WCursor *cursor)
{
    Q_UNUSED(cursor);
    W_D(WSeat);
    d->doNotifyFrame();
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

WSeatEventFilter::WSeatEventFilter(QObject *parent)
    : QObject(parent)
{

}

bool WSeatEventFilter::eventFilter(WSeat *, WSurface *, QInputEvent *event)
{
    event->ignore();
    return false;
}

bool WSeatEventFilter::eventFilter(WSeat *, QWindow *, QInputEvent *event)
{
    event->ignore();
    return false;
}

bool WSeatEventFilter::ignoredEventFilter(WSeat *, QWindow *, QInputEvent *event)
{
    event->ignore();
    return false;
}

WAYLIB_SERVER_END_NAMESPACE
