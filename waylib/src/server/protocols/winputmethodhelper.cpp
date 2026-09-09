// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodhelper.h"
#include "wtextinputv3_p.h"
#include "wtextinputv1_p.h"
#include "wtextinputv2_p.h"
#include "wtextinput_p.h"
#include "winputmethodv2_p.h"
#include "wvirtualkeyboardv1_p.h"
#include "winputpopupsurface.h"
#include "wseat.h"
#include "wsurface.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"
#include "wayliblogging.h"

#include <wlr_all.h>

#include <QQmlInfo>

#include <chrono>

WAYLIB_SERVER_BEGIN_NAMESPACE
namespace {

struct KeyboardStateSnapshot
{
    xkb_mod_mask_t depressed = 0;
    xkb_mod_mask_t latched = 0;
    xkb_mod_mask_t locked = 0;
    xkb_layout_index_t layout = 0;
};

KeyboardStateSnapshot snapshotKeyboardState(struct xkb_state *state)
{
    KeyboardStateSnapshot snap;
    snap.depressed = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
    snap.latched = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
    snap.locked = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
    snap.layout = xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_EFFECTIVE);
    return snap;
}

// Resolve a keysym to a (keycode, needsShift) pair from the given keymap and
// state. Uppercase/symbol keysyms are searched with Shift added to the
// currently effective modifier set.
bool findKeysymWithShift(struct xkb_keymap *keymap, struct xkb_state *state,
                         xkb_keysym_t keysym, xkb_keycode_t *keycodeOut, bool *shiftOut)
{
    const xkb_keycode_t first = xkb_keymap_min_keycode(keymap);
    const xkb_keycode_t last = xkb_keymap_max_keycode(keymap);
    for (xkb_keycode_t kc = first; kc <= last; ++kc) {
        if (xkb_state_key_get_one_sym(state, kc) == keysym) {
            *keycodeOut = kc;
            *shiftOut = false;
            return true;
        }
    }

    const xkb_mod_index_t shiftIndex = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
    if (shiftIndex == XKB_MOD_INVALID) {
        return false;
    }
    const KeyboardStateSnapshot snap = snapshotKeyboardState(state);
    struct xkb_state *shifted = xkb_state_new(keymap);
    if (!shifted) {
        return false;
    }
    bool found = false;
    if (xkb_state_update_mask(shifted, snap.depressed | (xkb_mod_mask_t(1) << shiftIndex),
                              snap.latched, snap.locked, 0, 0, snap.layout) == 0) {
        for (xkb_keycode_t kc = first; kc <= last; ++kc) {
            if (xkb_state_key_get_one_sym(shifted, kc) == keysym) {
                *keycodeOut = kc;
                *shiftOut = true;
                found = true;
                break;
            }
        }
    }
    xkb_state_unref(shifted);
    return found;
}

} // namespace

class Q_DECL_HIDDEN WInputMethodHelperPrivate : public WObjectPrivate,
                                                public WSeatKeyboardFilter
{
    W_DECLARE_PUBLIC(WInputMethodHelper)
public:
    explicit WInputMethodHelperPrivate(WServer *s, WSeat *st, WInputMethodHelper *qq)
        : WObjectPrivate(qq)
        , server(s)
        , seat(st)
        , inputMethodManagerV2(server->attach<WInputMethodManagerV2>())
        , textInputManagerV1(server->attach<WTextInputManagerV1>())
        , textInputManagerV2(server->attach<WTextInputManagerV2>())
        , textInputManagerV3(server->attach<WTextInputManagerV3>())
        , virtualKeyboardManagerV1(server->attach<WVirtualKeyboardManagerV1>())
        , enabledTextInput(nullptr)
        , activeInputMethod(nullptr)
        , activeKeyboardGrab(nullptr)
    {
        Q_ASSERT(server);
        Q_ASSERT(seat);
        Q_ASSERT(inputMethodManagerV2);
        Q_ASSERT(textInputManagerV1);
        Q_ASSERT(textInputManagerV2);
        Q_ASSERT(textInputManagerV3);
    }

    bool isInputMethodVirtualKeyboard(wlr_input_method_keyboard_grab_v2 *kgv2,
                                      WInputDevice *keyboard) const
    {
        if (!kgv2 || !keyboard)
            return false;
        auto *virtualKeyboard = wlr_input_device_get_virtual_keyboard(keyboard->handle());
        return virtualKeyboard && virtualKeyboard->resource && kgv2->resource
            && wl_resource_get_client(virtualKeyboard->resource)
                == wl_resource_get_client(kgv2->resource);
    }

    void setKeyboard(wlr_input_method_keyboard_grab_v2 *kgv2, WInputDevice *keyboard)
    {
        auto *kgHandle = kgv2;
        if (!kgHandle) {
            qCCritical(lcWlInputMethod) << "Failed to set keyboard for input method grab - grab handle is invalid"
                                      << kgv2 << "keyboard" << keyboard;
            return;
        }

        if (keyboard) {
            if (isInputMethodVirtualKeyboard(kgv2, keyboard))
                return;
            wlr_input_method_keyboard_grab_v2_set_keyboard(kgv2, wlr_keyboard_from_input_device(keyboard->handle()));
        } else {
            wlr_input_method_keyboard_grab_v2_set_keyboard(kgv2, nullptr);
        }
    }

    bool isTextInputEligible(WTextInput *ti) const
    {
        if (!ti || !enabledTextInputs.contains(ti) || !seat)
            return false;

        auto *focus = seat->keyboardFocusSurface();
        return focus && ti->seat() == seat && ti->focusedSurface() == focus
            && ti->waylandClient() == focus->waylandClient();
    }

    // While the input method is active its keyboard endpoint receives every
    // physical key and modifier event, regardless of whether the focused
    // surface owns a text input. This restores the sway/labwc routing model:
    // XWayland windows (which never create a text input at all) still deliver
    // keys to the input method while it remains activated, and unhandled keys
    // come back through the input method's own virtual keyboard. Text-input
    // matching stays enforced for activation (reconcileTextInput) and commit
    // routing (handleIMCommitted), so nothing is ever committed to the wrong
    // surface; commits without an eligible text input fall back to typing
    // through the virtual keyboard (see typeTextViaVirtualKeyboard).
    bool keyboardGrabBypassed(WSeat *eventSeat, WInputDevice *device) const
    {
        if (!seat || eventSeat != seat || !inputMethodActive
            || !activeKeyboardGrab || isInputMethodVirtualKeyboard(activeKeyboardGrab, device)) {
            return true;
        }
        // Drag-and-drop owns the seat keyboard grab; keys must follow the
        // drag instead of being diverted into the input method.
        auto *seatHandle = seat->handle();
        return !seatHandle || seatHandle->drag != nullptr;
    }

    bool filterKey(WSeat *eventSeat, WInputDevice *device, uint32_t keycode,
                   uint32_t state, uint32_t timestamp) override
    {
        if (keyboardGrabBypassed(eventSeat, device)) {
            return false;
        }

        setKeyboard(activeKeyboardGrab, device);
        wlr_input_method_keyboard_grab_v2_send_key(activeKeyboardGrab, timestamp, keycode, state);
        return true;
    }

    bool filterModifiers(WSeat *eventSeat, WInputDevice *device,
                         const wlr_keyboard_modifiers *modifiers) override
    {
        if (keyboardGrabBypassed(eventSeat, device)) {
            return false;
        }

        auto *keyboard = wlr_keyboard_from_input_device(device->handle());
        const bool keyboardChanged = activeKeyboardGrab->keyboard != keyboard;
        setKeyboard(activeKeyboardGrab, device);
        if (!keyboardChanged) {
            wlr_input_method_keyboard_grab_v2_send_modifiers(
                activeKeyboardGrab,
                const_cast<wlr_keyboard_modifiers *>(modifiers));
        }
        return true;
    }

    // Types text into the currently focused surface through the input
    // method's virtual keyboard, by mirroring the exact server-side effect of
    // a zwp_virtual_keyboard_v1 request. Returns the number of code points
    // typed; code points without a keymap mapping (e.g. CJK, which has no
    // physical key on any layout) are skipped, matching what any compositor
    // relying on virtual-keyboard delivery can express. Never logs the text.
    quint32 typeTextViaVirtualKeyboard(const QString &text);

    const QPointer<WServer> server;
    const QPointer<WSeat> seat;
    const QPointer<WInputMethodManagerV2> inputMethodManagerV2;
    const QPointer<WTextInputManagerV1> textInputManagerV1;
    const QPointer<WTextInputManagerV2> textInputManagerV2;
    const QPointer<WTextInputManagerV3> textInputManagerV3;
    const QPointer<WVirtualKeyboardManagerV1> virtualKeyboardManagerV1;
    WTextInput *enabledTextInput { nullptr };
    WInputMethodV2 *activeInputMethod { nullptr };
    wlr_input_method_keyboard_grab_v2 *activeKeyboardGrab {nullptr};
    bool inputMethodActive = false;
    bool updatingFocus = false;
    bool pendingResync = false;
    // True while the input method stays activated although the current seat
    // focus has no eligible text input (e.g. an XWayland window). The keyboard
    // endpoint keeps receiving keys and commits are typed through the virtual
    // keyboard instead of being routed to the (unfocused) anchor text input.
    bool anchorHeld = false;
    quint64 transitionSerial = 0;
    WScopedListener keyboardGrabDestroyListener;

    QList<WTextInput *> textInputs;
    QList<WTextInput *> enabledTextInputs;
    QList<WInputDevice *> virtualKeyboards;
    QList<WInputPopupSurface *> popupSurfaces;

};

quint32 WInputMethodHelperPrivate::typeTextViaVirtualKeyboard(const QString &text)
{
    if (!seat || !activeKeyboardGrab || text.isEmpty()) {
        return 0;
    }

    // Use the newest virtual keyboard owned by the input method client; it
    // carries the keymap the input method echoed from our keyboard endpoint.
    auto *grabClient = wl_resource_get_client(activeKeyboardGrab->resource);
    wlr_keyboard *virtualKeyboard = nullptr;
    for (auto it = virtualKeyboards.crbegin(); it != virtualKeyboards.crend(); ++it) {
        WInputDevice *device = *it;
        if (!device || !device->handle()) {
            continue;
        }
        auto *vk = wlr_input_device_get_virtual_keyboard(device->handle());
        if (vk && vk->has_keymap && vk->resource
            && wl_resource_get_client(vk->resource) == grabClient) {
            virtualKeyboard = &vk->keyboard;
            break;
        }
    }
    if (!virtualKeyboard || !virtualKeyboard->keymap || !virtualKeyboard->xkb_state) {
        return 0;
    }

    const KeyboardStateSnapshot snap = snapshotKeyboardState(virtualKeyboard->xkb_state);
    const xkb_mod_index_t shiftIndex =
        xkb_keymap_mod_get_index(virtualKeyboard->keymap, XKB_MOD_NAME_SHIFT);
    const xkb_mod_mask_t shiftBit = shiftIndex == XKB_MOD_INVALID
        ? xkb_mod_mask_t(0) : (xkb_mod_mask_t(1) << shiftIndex);

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const uint32_t timeMsec = static_cast<uint32_t>(now);

    quint32 typed = 0;
    for (int i = 0; i < text.size(); ++i) {
        uint32_t codePoint = text.at(i).unicode();
        if (QChar::isHighSurrogate(codePoint) && i + 1 < text.size()
            && QChar::isLowSurrogate(text.at(i + 1).unicode())) {
            codePoint = QChar::surrogateToUcs4(text.at(i), text.at(i + 1));
            ++i;
        }
        const xkb_keysym_t keysym = codePoint < 0x100
            ? static_cast<xkb_keysym_t>(codePoint)
            : static_cast<xkb_keysym_t>(0x01000000u + codePoint);

        xkb_keycode_t keycode = 0;
        bool needsShift = false;
        if (!findKeysymWithShift(virtualKeyboard->keymap, virtualKeyboard->xkb_state,
                                 keysym, &keycode, &needsShift)) {
            continue; // No physical key (e.g. CJK): cannot be typed this way.
        }

        const bool shiftAlreadyDown = snap.depressed & shiftBit;
        const bool pressShift = needsShift && !shiftAlreadyDown && shiftBit;
        if (pressShift) {
            wlr_keyboard_notify_modifiers(virtualKeyboard, snap.depressed | shiftBit,
                                          snap.latched, snap.locked, snap.layout);
        }

        // Mirror virtual_keyboard_handle_key(): the input method keeps the
        // modifier state of its virtual keyboard itself, so the synthetic key
        // events must not update it. Keycodes are in xkb (evdev + 8) format,
        // exactly like the input method's own key deliveries.
        wlr_keyboard_key_event keyEvent {};
        keyEvent.time_msec = timeMsec;
        keyEvent.keycode = keycode;
        keyEvent.update_state = false;
        keyEvent.state = WL_KEYBOARD_KEY_STATE_PRESSED;
        wlr_keyboard_notify_key(virtualKeyboard, &keyEvent);
        keyEvent.state = WL_KEYBOARD_KEY_STATE_RELEASED;
        wlr_keyboard_notify_key(virtualKeyboard, &keyEvent);

        if (pressShift) {
            wlr_keyboard_notify_modifiers(virtualKeyboard, snap.depressed,
                                          snap.latched, snap.locked, snap.layout);
        }
        ++typed;
    }
    return typed;
}

WInputMethodHelper::WInputMethodHelper(WServer *server, WSeat *seat)
    : QObject(server)
    , WObject(*new WInputMethodHelperPrivate(server, seat, this))
{
    W_D(WInputMethodHelper);
    Q_ASSERT(!d->seat->keyboardFilter());
    d->seat->setKeyboardFilter(d);
    QObject::connect(d->seat, &WSeat::keyboardFocusSurfaceChanged, this, &WInputMethodHelper::resendKeyboardFocus);
    QObject::connect(d->seat, &WSeat::keyboardChanged, this, [d] {
        if (auto *activeKG = d->activeKeyboardGrab)
            d->setKeyboard(activeKG, d->seat->keyboard());
    });
    connect(d->inputMethodManagerV2, &WInputMethodManagerV2::newInputMethod, this, &WInputMethodHelper::handleNewIMV2);
    connect(d->textInputManagerV3, &WTextInputManagerV3::newTextInput, this, &WInputMethodHelper::handleNewTI);
    connect(d->virtualKeyboardManagerV1, &WVirtualKeyboardManagerV1::newVirtualKeyboard, this, &WInputMethodHelper::handleNewVKV1);
    connect(d->textInputManagerV1, &WTextInputManagerV1::newTextInput, this, &WInputMethodHelper::handleNewTI);
    connect(d->textInputManagerV2, &WTextInputManagerV2::newTextInput, this, &WInputMethodHelper::handleNewTI);
}

WInputMethodHelper::~WInputMethodHelper()
{
    teardown();
    W_D(WInputMethodHelper);
    d->keyboardGrabDestroyListener.disconnect();
    if (d->seat && d->seat->keyboardFilter() == d)
        d->seat->setKeyboardFilter(nullptr);
    // The wrappers tracked below have no QObject parent and their destroy
    // callbacks capture this private; the native objects may outlive the
    // helper (which is deleted before the WServer), so release them here
    // while the helper is still alive to prevent callbacks from running
    // against freed state.

    // Active input method wrapper: normally destroyed from the native
    // destroy callback; detach that callback and release it explicitly.
    if (d->activeInputMethod)
        d->activeInputMethod->removeListeners(this);
    if (d->activeInputMethod) {
        delete d->activeInputMethod;
        d->activeInputMethod = nullptr;
    }

    // Input popup surface wrappers.
    const auto popupSurfaces = d->popupSurfaces;
    d->popupSurfaces.clear();
    for (auto *popup : popupSurfaces) {
        popup->removeListeners(this);
        delete popup;
    }

    // Virtual keyboard device wrappers (also detach them from the seat).
    const auto virtualKeyboards = d->virtualKeyboards;
    d->virtualKeyboards.clear();
    for (auto *keyboard : virtualKeyboards) {
        keyboard->removeListeners(this);
        if (d->seat) {
            if (d->seat->keyboard() == keyboard && d->seat->keyboardGroupKeyboard())
                d->seat->setKeyboard(d->seat->keyboardGroupKeyboard());
            d->seat->detachInputDevice(keyboard);
        }
        delete keyboard;
    }

    if (d->seat) d->seat->disconnect(this);
    if (d->inputMethodManagerV2) d->inputMethodManagerV2->disconnect(this);
    if (d->textInputManagerV1) d->textInputManagerV1->disconnect(this);
    if (d->textInputManagerV2) d->textInputManagerV2->disconnect(this);
    if (d->textInputManagerV3) d->textInputManagerV3->disconnect(this);
    if (d->virtualKeyboardManagerV1) d->virtualKeyboardManagerV1->disconnect(this);
}

WTextInput *WInputMethodHelper::enabledTextInput() const
{
    W_DC(WInputMethodHelper);
    return d->enabledTextInput;
}

void WInputMethodHelper::setEnabledTextInput(WTextInput *ti)
{
    W_D(WInputMethodHelper);
    if (d->enabledTextInput == ti)
        return;
    if (d->enabledTextInput) {
        disconnect(d->enabledTextInput, &WTextInput::committed, this, &WInputMethodHelper::handleFocusedTICommitted);
    }
    d->enabledTextInput = ti;
    if (ti) {
        updateAllPopupSurfaces(ti->cursorRect()); // Note: if this is necessary
        connect(ti, &WTextInput::committed, this, &WInputMethodHelper::handleFocusedTICommitted, Qt::UniqueConnection);
    }
}

WInputMethodV2 *WInputMethodHelper::inputMethod() const
{
    W_DC(WInputMethodHelper);
    return d->activeInputMethod;
}

WSurface *WInputMethodHelper::textInputFocusSurface() const
{
    auto ti = enabledTextInput();
    return ti ? ti->focusedSurface() : nullptr;
}

QRect WInputMethodHelper::textInputCursorRect() const
{
    auto ti = enabledTextInput();
    return ti ? ti->cursorRect() : QRect();
}

void WInputMethodHelper::setInputMethod(WInputMethodV2 *im)
{
    W_D(WInputMethodHelper);
    if (d->activeInputMethod == im)
        return;
    if (d->activeInputMethod)
        d->activeInputMethod->removeListeners(this);
    d->activeInputMethod = im;
    d->inputMethodActive = false;
    if (d->activeInputMethod)
        d->activeInputMethod->listeners(this)->add(&im->handle()->events.destroy, this,
            &WInputMethodHelper::handleActiveIMDestroyed);
}

void WInputMethodHelper::handleNewIMV2(wlr_input_method_v2 *imv2)
{
    W_D(WInputMethodHelper);
    // Reject input methods from other seats before wrapping: the wrapper
    // attaches native listeners in its constructor, so an early return
    // afterwards would leak it (the listeners stay registered on the native
    // handle and assert on its destroy).
    auto *imSeat = WSeat::fromHandle(imv2->seat);
    if (imSeat != d->seat)
        return;
    if (inputMethod()) {
        qCWarning(lcWlInputMethod) << "Ignore second creation of input on the same seat.";
        // sendUnavailable() destroys the native input method synchronously
        // (asserting its listener lists are empty afterwards); call it
        // before creating the wrapper so nothing is left attached.
        wlr_input_method_v2_send_unavailable(imv2);
        return;
    }
    auto wimv2 = new WInputMethodV2(imv2);
    setInputMethod(wimv2);
    connect(wimv2, &WInputMethodV2::committed, this, &WInputMethodHelper::handleIMCommitted);
    connect(wimv2, &WInputMethodV2::newKeyboardGrab, this, &WInputMethodHelper::handleNewKGV2);
    connect(wimv2, &WInputMethodV2::newPopupSurface, this, &WInputMethodHelper::handleNewIPSV2);
    qCInfo(lcWlInputMethod) << "Input method connected"
                             << "seat" << d->seat->name()
                             << "inputMethod" << wimv2;
    reconcileTextInput("input method connected");
}

void WInputMethodHelper::handleNewKGV2(wlr_input_method_keyboard_grab_v2 *kgv2)
{
    W_D(WInputMethodHelper);
    Q_ASSERT(d->seat);
    if (d->activeKeyboardGrab) {
        qCWarning(lcWlInputMethod) << "Replacing an existing input method keyboard endpoint"
                                    << "seat" << d->seat->name()
                                    << "oldGrab" << d->activeKeyboardGrab
                                    << "newGrab" << kgv2;
        d->keyboardGrabDestroyListener.disconnect();
    }

    d->activeKeyboardGrab = kgv2;
    // Prefer the (always non-IME) keyboard-group device: fcitx5 creates its
    // virtual keyboard right before the new keyboard endpoint, so at this
    // point the seat's current keyboard may still be that virtual keyboard
    // (which setKeyboard() would skip), leaving the new endpoint without
    // keymap/repeat-info/modifiers until the first physically filtered key.
    if (auto *groupKeyboard = d->seat->keyboardGroupKeyboard())
        d->setKeyboard(kgv2, groupKeyboard);
    else
        d->setKeyboard(kgv2, d->seat->keyboard());
    qCInfo(lcWlInputMethod) << "Input method keyboard endpoint available"
                             << "seat" << d->seat->name()
                             << "grab" << kgv2
                             << "activeTextInput" << d->enabledTextInput
                             << "anotherSeatGrabActive" << wlr_seat_keyboard_has_grab(d->seat->handle());

    d->keyboardGrabDestroyListener.init(&kgv2->events.destroy, this,
        [d, kgv2](void *) {
            if (d->activeKeyboardGrab != kgv2)
                return;
            qCInfo(lcWlInputMethod) << "Input method keyboard endpoint destroyed"
                                     << "seat" << d->seat->name()
                                     << "grab" << kgv2
                                     << "activeTextInput" << d->enabledTextInput;
            d->activeKeyboardGrab = nullptr;
            d->keyboardGrabDestroyListener.disconnect();
        });
}

void WInputMethodHelper::handleNewIPSV2(wlr_input_popup_surface_v2 *ipsv2)
{
    W_D(WInputMethodHelper);

    auto createPopupSurface = [this, d] (WSurface *focus, QRect cursorRect, wlr_input_popup_surface_v2 *popupSurface){
        auto surface = new WInputPopupSurface(popupSurface, focus);
        d->popupSurfaces.append(surface);
        updatePopupSurface(surface, cursorRect);
        Q_EMIT inputPopupSurfaceV2Added(surface);
        auto *listeners = surface->listeners(this);
        listeners->add(&popupSurface->events.destroy, this,
            [this, d, surface] (void *) {
            d->popupSurfaces.removeAll(surface);
            Q_EMIT inputPopupSurfaceV2Removed(surface);
            // Safe to destroy the wrapper from inside its own destroy
            // callback: the listener closure is reference-counted, and
            // ~WInputPopupSurface clears the reverse mapping while the
            // native popup storage is still valid.
            delete surface;
        });
    };
    auto ti = enabledTextInput();
    if (ti && ti->focusedSurface()) {
        createPopupSurface(ti->focusedSurface(), ti->cursorRect(), ipsv2);
    }
}

void WInputMethodHelper::handleNewVKV1(wlr_virtual_keyboard_v1 *vkv1)
{
    W_D(WInputMethodHelper);
    if (vkv1->seat != d->seat->handle())
        return;

    auto *keyboard = new WInputDevice(&vkv1->keyboard.base, true);
    d->virtualKeyboards.append(keyboard);
    d->seat->attachInputDevice(keyboard);
    auto *listeners = keyboard->listeners(this);
    listeners->add(&vkv1->keyboard.base.events.destroy, this,
        [this, d, keyboard] (void *) {
        if (d->seat) {
            // Switch seat keyboard to group before the virtual keyboard's
            // wlr_keyboard is destroyed. This removes handle_keyboard_destroy
            // listener from the virtual keyboard, preventing
            // wlr_seat_set_keyboard(NULL).
            if (d->seat->keyboard() == keyboard && d->seat->keyboardGroupKeyboard())
                d->seat->setKeyboard(d->seat->keyboardGroupKeyboard());
            d->seat->detachInputDevice(keyboard);
        }
        d->virtualKeyboards.removeOne(keyboard);
        // Safe to destroy the wrapper from inside its own destroy callback:
        // the listener closure is reference-counted and outlives the
        // emission, and ~WInputDevice clears the reverse mapping while the
        // native handle is still valid.
        delete keyboard;
    });
}

void WInputMethodHelper::resendKeyboardFocus()
{
    W_D(WInputMethodHelper);
    auto focus = d->seat->keyboardFocusSurface();
    if (d->updatingFocus) {
        // Re-entrant request (a signal fired while reconciling focus): mark it
        // pending instead of silently dropping the state change.
        d->pendingResync = true;
        return;
    }

    d->updatingFocus = true;
    qCDebug(lcWlInputMethod) << "Reconciling text-input focus"
                             << "seat" << d->seat->name()
                             << "focus" << focus
                             << "focusClient" << (focus ? focus->waylandClient() : nullptr)
                             << "textInputCount" << d->textInputs.size();

    // Send every obsolete leave before any enter. In particular, never emit a
    // same-client stale leave after a new enter: Qt's text-input-v2 client
    // clears its current surface on every leave event.
    // text-input-v1 has no seat before its first activate request; treat a
    // null-seat text input as belonging to this seat, matching master.
    for (auto *ti : std::as_const(d->textInputs)) {
        if (ti->seat() && ti->seat() != d->seat)
            continue;

        auto *tiFocus = ti->focusedSurface();
        const bool clientMatches = focus && ti->waylandClient() == focus->waylandClient();
        bool shouldFocus = clientMatches;
        if (qobject_cast<WTextInputV1 *>(ti))
            shouldFocus = shouldFocus && tiFocus == focus;

        if (tiFocus && (!shouldFocus || tiFocus != focus))
            ti->sendLeave();
    }

    if (focus) {
        for (auto *ti : std::as_const(d->textInputs)) {
            if ((ti->seat() && ti->seat() != d->seat)
                || ti->waylandClient() != focus->waylandClient()) {
                continue;
            }

            // text-input-v1 focus is client-managed: only acknowledge surfaces
            // whose activate request is still recorded. The record survives
            // server-driven leave events, so keyboard focus returning to an
            // activated v1 surface re-arms it here (sendEnter emits enabled()
            // even before the client sees the enter event).
            if (qobject_cast<WTextInputV1 *>(ti)) {
                if (ti->focusedSurface() == focus)
                    ti->sendEnter(focus);
                continue;
            }

            if (ti->focusedSurface() != focus)
                ti->sendEnter(focus);
        }
    }

    d->updatingFocus = false;
    reconcileTextInput("keyboard focus changed");

    if (d->pendingResync) {
        d->pendingResync = false;
        qCDebug(lcWlInputMethod) << "Re-running focus reconciliation after re-entrant request"
                                 << "seat" << d->seat->name();
        resendKeyboardFocus();
    }
}

void WInputMethodHelper::connectToTI(WTextInput *ti)
{
    qCDebug(lcWlInputMethod()) << "connect to text input" << ti;
    connect(ti, &WTextInput::enabled, this, &WInputMethodHelper::handleTIEnabled, Qt::UniqueConnection);
    connect(ti, &WTextInput::disabled, this, &WInputMethodHelper::handleTIDisabled, Qt::UniqueConnection);
    connect(ti, &WTextInput::requestLeave, ti, &WTextInput::sendLeave, Qt::UniqueConnection);
}

void WInputMethodHelper::disableTI(WTextInput *ti)
{
    W_D(WInputMethodHelper);
    Q_ASSERT(ti);
    d->enabledTextInputs.removeAll(ti);
    if (!d->updatingFocus)
        reconcileTextInput("text input disabled");
}

void WInputMethodHelper::handleNewTI(WTextInput *ti)
{
    W_D(WInputMethodHelper);
    qCDebug(lcWlInputMethod()) << "handle new text input" << ti
                              << "from seat:" << ti->seat();
    if (d->textInputs.contains(ti))
        return;
    d->textInputs.append(ti);
    connect(ti, &WTextInput::entityAboutToDestroy, this, [this, d, ti]{
        d->textInputs.removeAll(ti);
        disableTI(ti);
        ti->disconnect();
    }); // textInputs only save and traverse text inputs, do not handle connections
    // Whether this text input belongs to current seat or not, we should connect
    // its requestFocus signal for it might request focus from another seat to activate
    // itself here. For example, text input v1.
    connect(ti, &WTextInput::requestFocus, this, [this, ti, d]{
        if (!ti->seat() || ti->seat() == d->seat) {
            connectToTI(ti);
            resendKeyboardFocus();
        }
    });
    if (!ti->seat() || ti->seat() == d->seat) {
        connectToTI(ti);
        if (auto *focus = d->seat->keyboardFocusSurface();
            focus && ti->waylandClient() == focus->waylandClient()) {
            ti->sendEnter(focus);
        }
        reconcileTextInput("text input created");
    }
}

void WInputMethodHelper::handleTIEnabled()
{
    W_D(WInputMethodHelper);
    WTextInput *ti = qobject_cast<WTextInput*>(sender());
    Q_ASSERT(ti);
    d->enabledTextInputs.removeAll(ti);
    d->enabledTextInputs.append(ti);
    qCDebug(lcWlInputMethod) << "Text input became eligible candidate"
                             << "seat" << d->seat->name()
                             << "textInput" << ti
                             << "focusedSurface" << ti->focusedSurface()
                             << "seatFocus" << d->seat->keyboardFocusSurface();
    if (!d->updatingFocus)
        reconcileTextInput("text input enabled");
}

void WInputMethodHelper::handleTIDisabled()
{
    WTextInput *ti = qobject_cast<WTextInput*>(sender());
    Q_ASSERT(ti);
    disableTI(ti);
}

void WInputMethodHelper::reconcileTextInput(const char *reason)
{
    W_D(WInputMethodHelper);

    WTextInput *candidate = nullptr;
    for (auto it = d->enabledTextInputs.crbegin(); it != d->enabledTextInputs.crend(); ++it) {
        if (d->isTextInputEligible(*it)) {
            candidate = *it;
            break;
        }
    }

    auto *old = d->enabledTextInput;
    auto *im = d->activeInputMethod;
    const bool textInputChanged = old != candidate;
    // Sticky anchor: seat focus moved to a surface without an eligible text
    // input (XWayland window, plain terminal, ...) or is momentarily null
    // during a focus switch, while a previous text input is still enabled and
    // alive. Keep the input method activated so its keyboard endpoint keeps
    // receiving keys; commits are then typed via the virtual keyboard instead
    // of the anchor's surface. Holding through a null focus is safe: keys are
    // only ever delivered to an activated input method while the seat has a
    // focused surface, and the virtual-keyboard fallback never runs without
    // one (see handleIMCommitted). A destroyed or client-disabled anchor
    // (removed from enabledTextInputs) falls through to normal deactivation.
    const bool holdAnchor = !candidate && d->inputMethodActive && old
        && d->enabledTextInputs.contains(old);
    const bool wasAnchorHeld = d->anchorHeld;
    d->anchorHeld = holdAnchor;
    const bool shouldDeactivate = d->inputMethodActive && !holdAnchor
        && (!candidate || textInputChanged || !im);
    bool activatedNow = false;

    if (shouldDeactivate && im) {
        im->sendDeactivate();
        im->sendDone();
    }
    if (shouldDeactivate)
        d->inputMethodActive = false;

    if (textInputChanged && !holdAnchor)
        setEnabledTextInput(candidate);

    if (candidate && im && !d->inputMethodActive) {
        im->sendActivate();
        if (candidate->features().testFlag(IME::F_SurroundingText)) {
            im->sendSurroundingText(candidate->surroundingText(),
                                    candidate->surroundingCursor(),
                                    candidate->surroundingAnchor());
        }
        im->sendTextChangeCause(candidate->textChangeCause());
        if (candidate->features().testFlag(IME::F_ContentType)) {
            im->sendContentType(candidate->contentHints().toInt(), candidate->contentPurpose());
        }
        im->sendDone();
        d->inputMethodActive = true;
        activatedNow = true;
    }

    if (textInputChanged || shouldDeactivate || activatedNow || (holdAnchor != wasAnchorHeld)) {
        ++d->transitionSerial;
        qCInfo(lcWlInputMethod) << "Input method state reconciled"
                                 << "transition" << d->transitionSerial
                                 << "reason" << reason
                                 << "seat" << d->seat->name()
                                 << "oldTextInput" << old
                                 << "newTextInput" << candidate
                                 << "focus" << d->seat->keyboardFocusSurface()
                                 << "inputMethod" << im
                                 << "keyboardGrab" << d->activeKeyboardGrab
                                 << "active" << d->inputMethodActive
                                 << "anchorHeld" << d->anchorHeld
                                 << "candidateCount" << d->enabledTextInputs.size();
    }
}

void WInputMethodHelper::handleFocusedTICommitted()
{
    W_D(WInputMethodHelper);
    auto ti = enabledTextInput();
    if (!ti || !d->isTextInputEligible(ti)) {
        qCWarning(lcWlInputMethod) << "Discard commit from ineligible text input"
                                    << "seat" << d->seat->name()
                                    << "textInput" << ti
                                    << "textInputFocus" << (ti ? ti->focusedSurface() : nullptr)
                                    << "seatFocus" << d->seat->keyboardFocusSurface();
        reconcileTextInput("commit from ineligible text input");
        return;
    }
    qCDebug(lcWlInputMethod) << "Focused text input" << ti << "committed."
                            << "Cursor rectangle:" << ti->cursorRect();
    auto im = inputMethod();
    if (im && d->inputMethodActive) {
        IME::Features features = ti->features();
        if (features.testFlag(IME::F_SurroundingText)) {
            im->sendSurroundingText(ti->surroundingText(), ti->surroundingCursor(), ti->surroundingAnchor());
        }
        im->sendTextChangeCause(ti->textChangeCause());
        if (features.testFlag(IME::F_ContentType)) {
            im->sendContentType(ti->contentHints().toInt(), ti->contentPurpose());
        }
        im->sendDone();
    }
    updateAllPopupSurfaces(ti->cursorRect());
    Q_EMIT textInputCursorRectChanged(ti->cursorRect());
}

void WInputMethodHelper::handleIMCommitted()
{
    W_D(WInputMethodHelper);
    auto im = inputMethod();
    Q_ASSERT(im);
    auto ti = enabledTextInput();
    if (d->inputMethodActive && d->isTextInputEligible(ti)) {
        ti->handleIMCommitted(im);
    } else if (d->inputMethodActive && d->anchorHeld && d->seat
               && d->seat->keyboardFocusSurface()) {
        // Focus is on a surface without a text input (e.g. XWayland) while the
        // input method stays activated on its anchor. Type the commit through
        // the input method's own virtual keyboard so it lands on the focused
        // surface; never route it to the unfocused anchor text input.
        const quint32 typed = d->typeTextViaVirtualKeyboard(im->commitString());
        ++d->transitionSerial;
        qCInfo(lcWlInputMethod) << "Anchor-held commit delivered via virtual-keyboard typing"
                                << "transition" << d->transitionSerial
                                << "seat" << d->seat->name()
                                << "textInput" << ti
                                << "seatFocus" << d->seat->keyboardFocusSurface()
                                << "typedCodePoints" << typed;
        if (typed == 0) {
            qCDebug(lcWlInputMethod) << "Anchor-held commit could not be delivered; no typeable code point"
                                     << "seat" << d->seat->name()
                                     << "virtualKeyboard" << !d->virtualKeyboards.isEmpty();
        }
    } else {
        qCWarning(lcWlInputMethod) << "Discard input method commit without an eligible text input"
                                    << "seat" << d->seat->name()
                                    << "textInput" << ti
                                    << "seatFocus" << d->seat->keyboardFocusSurface();
    }
}

void WInputMethodHelper::handleActiveIMDestroyed()
{
    W_D(WInputMethodHelper);
    auto im = inputMethod();
    Q_ASSERT(im);
    qCInfo(lcWlInputMethod) << "Input method disconnected"
                             << "seat" << d->seat->name()
                             << "inputMethod" << im
                             << "activeTextInput" << d->enabledTextInput;
    setInputMethod(nullptr);
    delete im;
    reconcileTextInput("input method disconnected");
}

void WInputMethodHelper::updateAllPopupSurfaces(QRect cursorRect)
{
    for (auto popup : std::as_const(d_func()->popupSurfaces)) {
        updatePopupSurface(popup, cursorRect);
    }
}

void WInputMethodHelper::updatePopupSurface(WInputPopupSurface *popup, QRect cursorRect)
{
    Q_ASSERT(popup->handle());
    popup->sendCursorRect(cursorRect);
}

WAYLIB_SERVER_END_NAMESPACE
