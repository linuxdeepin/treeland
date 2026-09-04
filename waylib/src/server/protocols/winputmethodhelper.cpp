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

WAYLIB_SERVER_BEGIN_NAMESPACE
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

    bool filterKey(WSeat *eventSeat, WInputDevice *device, uint32_t keycode,
                   uint32_t state, uint32_t timestamp) override
    {
        if (eventSeat != seat || !inputMethodActive || !activeKeyboardGrab
            || !isTextInputEligible(enabledTextInput)
            || isInputMethodVirtualKeyboard(activeKeyboardGrab, device)) {
            return false;
        }

        setKeyboard(activeKeyboardGrab, device);
        wlr_input_method_keyboard_grab_v2_send_key(activeKeyboardGrab, timestamp, keycode, state);
        return true;
    }

    bool filterModifiers(WSeat *eventSeat, WInputDevice *device,
                         const wlr_keyboard_modifiers *modifiers) override
    {
        if (eventSeat != seat || !inputMethodActive || !activeKeyboardGrab
            || !isTextInputEligible(enabledTextInput)
            || isInputMethodVirtualKeyboard(activeKeyboardGrab, device)) {
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
    quint64 transitionSerial = 0;
    WScopedListener keyboardGrabDestroyListener;

    QList<WTextInput *> textInputs;
    QList<WTextInput *> enabledTextInputs;
    QList<WInputDevice *> virtualKeyboards;
    QList<WInputPopupSurface *> popupSurfaces;

};

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
    d->setKeyboard(kgv2, d->seat->keyboard());
    qCInfo(lcWlInputMethod) << "Input method keyboard endpoint available"
                             << "seat" << d->seat->name()
                             << "grab" << kgv2
                             << "activeTextInput" << d->enabledTextInput
                             << "structuralGrab" << d->seat->handle()->keyboard_state.grab;

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
    if (d->updatingFocus)
        return;

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

            // text-input-v1 focus is client-managed: only acknowledge the
            // exact surface supplied by its activate request.
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
    const bool shouldDeactivate = d->inputMethodActive && (!candidate || textInputChanged || !im);
    bool activatedNow = false;

    if (shouldDeactivate && im) {
        im->sendDeactivate();
        im->sendDone();
    }
    if (shouldDeactivate)
        d->inputMethodActive = false;

    if (textInputChanged)
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

    if (textInputChanged || shouldDeactivate || activatedNow) {
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
