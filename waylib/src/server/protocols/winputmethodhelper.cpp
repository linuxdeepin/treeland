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

#include <QPointer>
#include <QSet>

#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {
// A keyboard focus surface may be an xdg popup opened from a text input
// surface.  Such a popup owns the seat keyboard while its grab is active, but
// it must not end the input-method session of the parent text input: the
// candidate window and the composition belong to the parent.  Walk the popup
// chain and accept only a popup that is anchored to the same text-input
// surface (nested popups included).
bool isXdgPopupOfSurface(WSurface *popup, WSurface *parent)
{
    if (!popup || !parent || popup == parent || !popup->handle() || !parent->handle())
        return false;

    wlr_surface *surface = popup->handle();
    while (surface) {
        auto *xdgSurface = wlr_xdg_surface_try_from_wlr_surface(surface);
        if (!xdgSurface || xdgSurface->role != WLR_XDG_SURFACE_ROLE_POPUP || !xdgSurface->popup)
            return false;

        surface = xdgSurface->popup->parent;
        if (surface == parent->handle())
            return true;
    }

    return false;
}
}

class Q_DECL_HIDDEN WInputMethodHelperPrivate : public WObjectPrivate,
                                               public WSeatKeyboardFilter
{
    W_DECLARE_PUBLIC(WInputMethodHelper)
public:
    explicit WInputMethodHelperPrivate(WServer *s, WSeat *st, WInputMethodHelper *qq,
                                       const WInputMethodHelper::Protocols &protocols)
        : WObjectPrivate(qq)
        , server(s)
        , seat(st)
        , inputMethodManagerV2(protocols.inputMethod)
        , textInputManagerV1(protocols.textInputV1)
        , textInputManagerV2(protocols.textInputV2)
        , textInputManagerV3(protocols.textInputV3)
        , virtualKeyboardManagerV1(protocols.virtualKeyboard)
        , enabledTextInput(nullptr)
        , activeInputMethod(nullptr)
        , activeKeyboardGrab(nullptr)
        , inputMethodActive(false)
        , updatingFocus(false)
        , pendingResync(false)
        , drainingPopupSurfaces(false)
    {
        Q_ASSERT(server);
        Q_ASSERT(seat);
        Q_ASSERT(inputMethodManagerV2);
        Q_ASSERT(textInputManagerV1);
        Q_ASSERT(textInputManagerV2);
        Q_ASSERT(textInputManagerV3);
        Q_ASSERT(virtualKeyboardManagerV1);
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
        if (!focus || ti->seat() != seat)
            return false;

        auto *tiFocus = ti->focusedSurface();
        if (!tiFocus || ti->waylandClient() != focus->waylandClient())
            return false;

        return tiFocus == focus || isXdgPopupOfSurface(focus, tiFocus);
    }

    // Newest enabled text input that currently matches the keyboard focus, if
    // any. Activation, key routing, popup anchoring and commit routing all use
    // this same predicate so they cannot disagree about the current owner.
    WTextInput *findEligibleTextInput() const
    {
        for (auto it = enabledTextInputs.crbegin(); it != enabledTextInputs.crend(); ++it) {
            if (isTextInputEligible(*it))
                return *it;
        }
        return nullptr;
    }

    // Text-input focus is not the same thing as the seat's raw keyboard focus:
    // while a popup opened from the anchor owns the keyboard, the text input
    // must stay entered on its parent surface, otherwise the client loses the
    // composition around every menu.  Also keep the anchor across the
    // momentary null focus of a window switch; reconcileTextInput() has the
    // matching hold rule.
    WSurface *textInputFocusForCurrentSeatFocus() const
    {
        if (!seat)
            return nullptr;

        auto *seatFocus = seat->keyboardFocusSurface();
        if (auto *eligible = findEligibleTextInput())
            return eligible->focusedSurface();

        if (!seatFocus && inputMethodActive && enabledTextInput
            && enabledTextInputs.contains(enabledTextInput)) {
            return enabledTextInput->focusedSurface();
        }

        return seatFocus;
    }

    void notifyTextInputFocusSurfaceChanged()
    {
        auto *eligible = findEligibleTextInput();
        auto *focus = eligible ? eligible->focusedSurface() : nullptr;
        if (focus == lastTextInputFocusSurface)
            return;
        lastTextInputFocusSurface = focus;
        if (!seat)
            return;
        W_Q(WInputMethodHelper);
        Q_EMIT q->textInputFocusSurfaceChanged(focus);
    }

    // Re-send the physical modifier state after the input method stops owning
    // the keyboard.  Modifier events consumed by filterModifiers() are not
    // visible to the client, and without this a modifier released during IME
    // composition stays latched in the client's xkb state.
    void resyncKeyboardState()
    {
        if (!seat)
            return;

        auto *device = seat->keyboardGroupKeyboard();
        if (!device)
            device = seat->keyboard();
        if (!device)
            return;

        auto *keyboard = wlr_keyboard_from_input_device(device->handle());
        if (!keyboard)
            return;

        wlr_seat_keyboard_send_modifiers(seat->handle(), &keyboard->modifiers);
    }

    void createPopupSurface(WSurface *focus, const QRect &cursorRect,
                            wlr_input_popup_surface_v2 *native);
    void drainPendingPopupSurfaces();

    bool keyboardGrabBypassed(WSeat *eventSeat, WInputDevice *device) const
    {
        if (!seat || eventSeat != seat || !inputMethodActive
            || !activeKeyboardGrab || isInputMethodVirtualKeyboard(activeKeyboardGrab, device)) {
            return true;
        }
        if (!findEligibleTextInput())
            return true;

        // An xdg-popup or drag-and-drop grab owns the seat keyboard while it is
        // active: keys must follow the grab (menus, combo boxes, DnD) instead
        // of being diverted into the input method.  The input method stays
        // activated for a popup anchored to the text input, so composition
        // survives it.
        auto *seatHandle = seat->handle();
        return !seatHandle || wlr_seat_keyboard_has_grab(seatHandle);
    }

    bool filterKey(WSeat *eventSeat, WInputDevice *device, uint32_t keycode,
                   uint32_t state, uint32_t timestamp) override
    {
        if (keyboardGrabBypassed(eventSeat, device))
            return false;

        // A release for a key whose press was delivered to the client before
        // the input method became active must stay with the client, otherwise
        // the key remains logically pressed there.
        if (state == WL_KEYBOARD_KEY_STATE_RELEASED && !forwardedKeys.contains(keycode))
            return false;

        setKeyboard(activeKeyboardGrab, device);
        wlr_input_method_keyboard_grab_v2_send_key(activeKeyboardGrab, timestamp, keycode, state);

        if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
            forwardedKeys.insert(keycode);
        else
            forwardedKeys.remove(keycode);
        return true;
    }

    bool filterModifiers(WSeat *eventSeat, WInputDevice *device,
                         const wlr_keyboard_modifiers *modifiers) override
    {
        if (keyboardGrabBypassed(eventSeat, device))
            return false;

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
    bool drainingPopupSurfaces = false;
    quint64 transitionSerial = 0;
    QPointer<WSurface> lastTextInputFocusSurface;
    WScopedListener keyboardGrabDestroyListener;

    QList<WTextInput *> textInputs;
    QList<WTextInput *> enabledTextInputs;
    QList<WInputDevice *> virtualKeyboards;
    QList<WInputPopupSurface *> popupSurfaces;
    QSet<uint32_t> forwardedKeys;

    struct PendingPopupSurface {
        wlr_input_popup_surface_v2 *handle = nullptr;
        WScopedListener destroyListener;
    };
    QList<PendingPopupSurface *> pendingPopupSurfaces;
};

void WInputMethodHelperPrivate::createPopupSurface(WSurface *focus, const QRect &cursorRect,
                                                   wlr_input_popup_surface_v2 *native)
{
    if (!focus || !native)
        return;

    W_Q(WInputMethodHelper);
    auto surface = new WInputPopupSurface(native, focus);
    popupSurfaces.append(surface);
    // Install the native destroy listener before emitting anything: the
    // compositor handler may run client/plugin code that destroys the input
    // method or its popup.  Emitting first would leave a wrapper with a
    // dangling native handle and no way to observe the destruction.
    auto *listeners = surface->listeners(q);
    listeners->add(&native->events.destroy, q,
        [q, this, surface] (void *) {
        popupSurfaces.removeAll(surface);
        Q_EMIT q->inputPopupSurfaceV2Removed(surface);
        // Safe to destroy the wrapper from inside its own destroy callback:
        // the listener closure is reference-counted, and
        // ~WInputPopupSurface clears the reverse mapping while the native
        // popup storage is still valid.
        delete surface;
    });
    q->updatePopupSurface(surface, cursorRect);
    Q_EMIT q->inputPopupSurfaceV2Added(surface);
}

void WInputMethodHelperPrivate::drainPendingPopupSurfaces()
{
    if (drainingPopupSurfaces || pendingPopupSurfaces.isEmpty())
        return;

    auto *ti = findEligibleTextInput();
    if (!ti || !ti->focusedSurface())
        return;

    drainingPopupSurfaces = true;
    while (!pendingPopupSurfaces.isEmpty()) {
        auto *entry = pendingPopupSurfaces.takeFirst();
        // Take ownership before creating anything: inputPopupSurfaceV2Added
        // runs compositor code synchronously and may destroy other pending
        // popups.  The remaining entries stay in the list and their destroy
        // listeners are still connected, so they can be removed safely.
        entry->destroyListener.disconnect();
        auto *native = entry->handle;
        delete entry;

        ti = findEligibleTextInput();
        if (!ti || !ti->focusedSurface()) {
            // The focus changed while a popup was being created.  Keep the
            // native popup for the next reconciliation instead of anchoring it
            // to a surface that no longer owns the input method.
            if (native) {
                auto *pending = new PendingPopupSurface;
                pending->handle = native;
                pending->destroyListener.init(&native->events.destroy, q_func(),
                    [this, pending](void *) {
                    pendingPopupSurfaces.removeAll(pending);
                    pending->destroyListener.disconnect();
                    delete pending;
                });
                pendingPopupSurfaces.prepend(pending);
            }
            break;
        }

        if (native) {
            createPopupSurface(ti->focusedSurface(), ti->cursorRect(), native);
        }
    }
    drainingPopupSurfaces = false;
}

WInputMethodHelper::WInputMethodHelper(WServer *server, WSeat *seat, const Protocols &protocols)
    : QObject(server)
    , WObject(*new WInputMethodHelperPrivate(server, seat, this, protocols))
{
    W_D(WInputMethodHelper);
    if (d->seat->keyboardFilter() && d->seat->keyboardFilter() != d) {
        qCWarning(lcWlInputMethod) << "Seat already has a keyboard filter; replacing it"
                                   << "seat" << d->seat->name();
    }
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

    if (d->activeInputMethod)
        d->activeInputMethod->removeListeners(this);
    if (d->activeInputMethod) {
        delete d->activeInputMethod;
        d->activeInputMethod = nullptr;
    }

    const auto popupSurfaces = d->popupSurfaces;
    d->popupSurfaces.clear();
    for (auto *popup : popupSurfaces) {
        popup->removeListeners(this);
        delete popup;
    }

    const auto pendingPopupSurfaces = d->pendingPopupSurfaces;
    d->pendingPopupSurfaces.clear();
    for (auto *pending : pendingPopupSurfaces) {
        pending->destroyListener.disconnect();
        delete pending;
    }

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

    if (d->seat) {
        d->seat->removeListeners(this);
        d->seat->disconnect(this);
    }
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
        updateAllPopupSurfaces(ti->cursorRect());
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
    W_DC(WInputMethodHelper);
    auto *ti = d->findEligibleTextInput();
    return ti ? ti->focusedSurface() : nullptr;
}

QRect WInputMethodHelper::textInputCursorRect() const
{
    W_DC(WInputMethodHelper);
    auto *ti = d->findEligibleTextInput();
    return ti ? ti->cursorRect() : QRect();
}

bool WInputMethodHelper::inputMethodConnected() const
{
    W_DC(WInputMethodHelper);
    return d->activeInputMethod != nullptr;
}

void WInputMethodHelper::deactivate()
{
    W_D(WInputMethodHelper);
    if (!d->inputMethodActive)
        return;
    if (auto *im = d->activeInputMethod) {
        im->sendDeactivate();
        im->sendDone();
    }
    d->inputMethodActive = false;
    d->forwardedKeys.clear();
    d->resyncKeyboardState();
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
    d->forwardedKeys.clear();
    if (d->activeInputMethod)
        d->activeInputMethod->listeners(this)->add(&im->handle()->events.destroy, this,
            &WInputMethodHelper::handleActiveIMDestroyed);
    Q_EMIT inputMethodChanged(im != nullptr);
}

void WInputMethodHelper::handleNewIMV2(wlr_input_method_v2 *imv2)
{
    W_D(WInputMethodHelper);
    if (!d->seat || !imv2)
        return;
    // Reject input methods from other seats before wrapping: the wrapper
    // attaches native listeners in its constructor, so an early return
    // afterwards would leak it.
    auto *imSeat = WSeat::fromHandle(imv2->seat);
    if (!imSeat || d->seat != imSeat)
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
    // Re-announce keyboard focus so a text input that was left by a previous
    // input method is re-armed even though the seat focus did not change.
    resendKeyboardFocus();
}

void WInputMethodHelper::handleNewKGV2(wlr_input_method_keyboard_grab_v2 *kgv2)
{
    W_D(WInputMethodHelper);
    if (!d->seat || !kgv2)
        return;
    if (d->activeKeyboardGrab) {
        qCWarning(lcWlInputMethod) << "Replacing an existing input method keyboard endpoint"
                                   << "seat" << d->seat->name()
                                   << "oldGrab" << d->activeKeyboardGrab
                                   << "newGrab" << kgv2;
        d->keyboardGrabDestroyListener.disconnect();
        // The replacement endpoint did not see these presses; let the client
        // receive their releases rather than routing them to a fresh client.
        d->forwardedKeys.clear();
    }

    d->activeKeyboardGrab = kgv2;
    // Bind the endpoint to the seat's current keyboard: that is the keyboard
    // the keys actually arrive from.  The only keyboard that must not be used
    // is the input method's own virtual keyboard (fcitx5 creates it right
    // before the endpoint); fall back to the keyboard group in that case.
    auto *keyboard = d->seat->keyboard();
    if (!keyboard || d->isInputMethodVirtualKeyboard(kgv2, keyboard))
        keyboard = d->seat->keyboardGroupKeyboard();
    d->setKeyboard(kgv2, keyboard);
    qCDebug(lcWlInputMethod) << "Input method keyboard endpoint available"
                             << "seat" << d->seat->name()
                             << "grab" << kgv2
                             << "activeTextInput" << d->enabledTextInput;

    d->keyboardGrabDestroyListener.init(&kgv2->events.destroy, this,
        [d, kgv2](void *) {
            if (d->activeKeyboardGrab != kgv2)
                return;
            qCDebug(lcWlInputMethod) << "Input method keyboard endpoint destroyed"
                                     << "seat" << (d->seat ? d->seat->name() : QString())
                                     << "grab" << kgv2;
            d->activeKeyboardGrab = nullptr;
            d->forwardedKeys.clear();
            d->keyboardGrabDestroyListener.disconnect();
        });
}

void WInputMethodHelper::handleNewIPSV2(wlr_input_popup_surface_v2 *ipsv2)
{
    W_D(WInputMethodHelper);
    if (!ipsv2)
        return;

    auto *ti = d->findEligibleTextInput();
    if (!ti || !ti->focusedSurface()) {
        auto *entry = new WInputMethodHelperPrivate::PendingPopupSurface;
        entry->handle = ipsv2;
        entry->destroyListener.init(&ipsv2->events.destroy, this, [d, entry](void *) {
            d->pendingPopupSurfaces.removeAll(entry);
            entry->destroyListener.disconnect();
            delete entry;
        });
        d->pendingPopupSurfaces.append(entry);
        qCDebug(lcWlInputMethod) << "Input method popup surface pending: no eligible text input yet"
                                 << "seat" << (d->seat ? d->seat->name() : QString())
                                 << "popupSurface" << ipsv2
                                 << "activeTextInput" << d->enabledTextInput;
        return;
    }

    d->createPopupSurface(ti->focusedSurface(), ti->cursorRect(), ipsv2);
}

void WInputMethodHelper::handleNewVKV1(wlr_virtual_keyboard_v1 *vkv1)
{
    W_D(WInputMethodHelper);
    if (!d->seat || !vkv1 || WSeat::fromHandle(vkv1->seat) != d->seat)
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
        delete keyboard;
    });
}

void WInputMethodHelper::resendKeyboardFocus()
{
    W_D(WInputMethodHelper);
    if (!d->seat)
        return;
    auto *seatFocus = d->seat->keyboardFocusSurface();
    auto *focus = d->textInputFocusForCurrentSeatFocus();

    if (d->updatingFocus) {
        d->pendingResync = true;
        return;
    }

    d->updatingFocus = true;
    qCDebug(lcWlInputMethod) << "Reconciling text-input focus"
                             << "seat" << d->seat->name()
                             << "seatFocus" << seatFocus
                             << "textFocus" << focus
                             << "focusClient" << (focus ? focus->waylandClient() : nullptr)
                             << "textInputCount" << d->textInputs.size();

    // Send every obsolete leave before any enter.  In particular, never emit a
    // same-client stale leave after a new enter: Qt's text-input-v2 client
    // clears its current surface on every leave event.
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
            // whose activate request is still recorded.
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

void WInputMethodHelper::notifyLeave()
{
    W_D(WInputMethodHelper);
    for (auto *ti : std::as_const(d->textInputs)) {
        if (ti->seat() != d->seat)
            continue;
        if (ti->focusedSurface())
            ti->sendLeave();
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
    if (!d->seat)
        return;
    qCDebug(lcWlInputMethod()) << "handle new text input" << ti
                               << "from seat:" << ti->seat();
    if (d->textInputs.contains(ti))
        return;
    d->textInputs.append(ti);
    connect(ti, &WTextInput::entityAboutToDestroy, this, [this, d, ti]{
        d->textInputs.removeAll(ti);
        disableTI(ti);
        // Disconnect only this helper.  A text-input-v1 object without a seat
        // is tracked by every seat's helper, and a broad ti->disconnect()
        // would prevent the other helpers from cleaning up their pointers.
        disconnect(ti, nullptr, this, nullptr);
    });
    // text-input-v1 has no seat until its first activate request.  Connect its
    // requestFocus for every helper; the lambda filters by seat.
    connect(ti, &WTextInput::requestFocus, this, [this, ti, d]{
        if (!ti->seat() || ti->seat() == d->seat) {
            connectToTI(ti);
            resendKeyboardFocus();
        }
    });
    if (!ti->seat() || ti->seat() == d->seat) {
        connectToTI(ti);
        if (auto *focus = d->textInputFocusForCurrentSeatFocus();
            focus && ti->waylandClient() == focus->waylandClient()) {
            ti->sendEnter(focus);
        }
        reconcileTextInput("text input created");
    }
}

void WInputMethodHelper::handleTIEnabled()
{
    W_D(WInputMethodHelper);
    if (!d->seat)
        return;
    WTextInput *ti = qobject_cast<WTextInput*>(sender());
    Q_ASSERT(ti);
    if (!ti)
        return;

    d->enabledTextInputs.removeAll(ti);
    // A text-input-v1 object has no seat before activate; it becomes eligible
    // only after the activate request, which emits requestFocus and re-enters
    // here once the seat is known.
    if (ti->seat() == d->seat) {
        d->enabledTextInputs.append(ti);
    }
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
    if (ti)
        disableTI(ti);
}

void WInputMethodHelper::reconcileTextInput(const char *reason)
{
    W_D(WInputMethodHelper);

    WTextInput *candidate = d->findEligibleTextInput();

    auto *old = d->enabledTextInput;
    auto *im = d->activeInputMethod;
    auto *focus = d->seat ? d->seat->keyboardFocusSurface() : nullptr;
    const bool textInputChanged = old != candidate;
    // Sticky anchor for the momentary null focus of a window switch.  A
    // concrete focus surface without an eligible text input deactivates.
    const bool holdAnchor = !candidate && !focus && d->inputMethodActive && old
        && d->enabledTextInputs.contains(old);
    const bool shouldDeactivate = d->inputMethodActive && !holdAnchor
        && (!candidate || textInputChanged || !im);
    bool activatedNow = false;

    if (shouldDeactivate && im) {
        im->sendDeactivate();
        im->sendDone();
    }
    if (shouldDeactivate) {
        d->inputMethodActive = false;
        d->forwardedKeys.clear();
        d->resyncKeyboardState();
    }

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

    if (textInputChanged || shouldDeactivate || activatedNow || holdAnchor) {
        ++d->transitionSerial;
        qCDebug(lcWlInputMethod) << "Input method state reconciled"
                                 << "transition" << d->transitionSerial
                                 << "reason" << reason
                                 << "seat" << (d->seat ? d->seat->name() : QString())
                                 << "oldTextInput" << old
                                 << "newTextInput" << candidate
                                 << "focus" << focus
                                 << "inputMethod" << im
                                 << "keyboardGrab" << d->activeKeyboardGrab
                                 << "active" << d->inputMethodActive
                                 << "anchorHeld" << holdAnchor
                                 << "candidateCount" << d->enabledTextInputs.size();
    }

    d->notifyTextInputFocusSurfaceChanged();
    d->drainPendingPopupSurfaces();
}

void WInputMethodHelper::handleFocusedTICommitted()
{
    W_D(WInputMethodHelper);
    auto ti = enabledTextInput();
    if (!ti || !d->isTextInputEligible(ti)) {
        qCWarning(lcWlInputMethod) << "Discard commit from ineligible text input"
                                   << "seat" << (d->seat ? d->seat->name() : QString())
                                   << "textInput" << ti
                                   << "textInputFocus" << (ti ? ti->focusedSurface() : nullptr)
                                   << "seatFocus" << (d->seat ? d->seat->keyboardFocusSurface() : nullptr);
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
    } else {
        qCWarning(lcWlInputMethod) << "Discard input method commit without an eligible text input"
                                   << "seat" << (d->seat ? d->seat->name() : QString())
                                   << "textInput" << ti
                                   << "seatFocus" << (d->seat ? d->seat->keyboardFocusSurface() : nullptr);
    }
}

void WInputMethodHelper::handleActiveIMDestroyed()
{
    W_D(WInputMethodHelper);
    auto im = inputMethod();
    Q_ASSERT(im);
    qCInfo(lcWlInputMethod) << "Input method disconnected"
                            << "seat" << (d->seat ? d->seat->name() : QString())
                            << "inputMethod" << im
                            << "activeTextInput" << d->enabledTextInput;
    setInputMethod(nullptr);
    delete im;
    d->forwardedKeys.clear();
    // The client has been receiving neither keys nor modifier updates while
    // the input method owned the keyboard.  Restore the physical modifier
    // state before the text input resumes normal key handling.
    d->resyncKeyboardState();
    notifyLeave();
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
