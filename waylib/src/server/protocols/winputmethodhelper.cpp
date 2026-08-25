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

#include <memory>

#include <QQmlInfo>

WAYLIB_SERVER_BEGIN_NAMESPACE
struct Q_DECL_HIDDEN GrabHandlerArg {
    const WInputMethodHelper *const helper;
    wlr_input_method_keyboard_grab_v2 *grab;
};

void handleKey(struct wlr_seat_keyboard_grab *grab, uint32_t time_msec, uint32_t key, uint32_t state)
{
    auto arg = reinterpret_cast<GrabHandlerArg*>(grab->data);
    if (!arg->grab) {
        qCCritical(lcWlInputMethod) << "Ignore key event for destroyed input method keyboard grab"
                                  << "key" << key << "state" << state;
        return;
    }
    for (auto vk: arg->helper->virtualKeyboards()) {
        if (wlr_keyboard_from_input_device(vk->handle()) == grab->seat->keyboard_state.keyboard) {
            auto *virtualKeyboard = wlr_input_device_get_virtual_keyboard(vk->handle());
            if (virtualKeyboard && virtualKeyboard->resource && arg->grab->resource
                && wl_resource_get_client(virtualKeyboard->resource)
                    == wl_resource_get_client(arg->grab->resource)) {
                grab->seat->keyboard_state.default_grab->interface->key(grab, time_msec, key, state);
                return;
            }
        }
    }
    wlr_input_method_keyboard_grab_v2_send_key(arg->grab, time_msec, key, state);
}

void handleModifiers(struct wlr_seat_keyboard_grab *grab, const struct wlr_keyboard_modifiers *modifiers)
{
    auto arg = reinterpret_cast<GrabHandlerArg*>(grab->data);
    if (!arg->grab) {
        qCCritical(lcWlInputMethod) << "Ignore modifiers for destroyed input method keyboard grab";
        return;
    }
    for (auto vk: arg->helper->virtualKeyboards()) {
        if (wlr_keyboard_from_input_device(vk->handle()) == grab->seat->keyboard_state.keyboard) {
            auto *virtualKeyboard = wlr_input_device_get_virtual_keyboard(vk->handle());
            if (virtualKeyboard && virtualKeyboard->resource && arg->grab->resource
                && wl_resource_get_client(virtualKeyboard->resource)
                    == wl_resource_get_client(arg->grab->resource)) {
                grab->seat->keyboard_state.default_grab->interface->modifiers(grab, modifiers);
                return;
            }
        }
    }
    wlr_input_method_keyboard_grab_v2_send_modifiers(arg->grab, const_cast<struct wlr_keyboard_modifiers *>(modifiers));
}

class Q_DECL_HIDDEN WInputMethodHelperPrivate : public WObjectPrivate
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
        , keyboardGrab{}
        , grabInterface{}
        , handlerArg({.helper = qq, .grab = nullptr})
    {
        Q_ASSERT(server);
        Q_ASSERT(seat);
        Q_ASSERT(inputMethodManagerV2);
        Q_ASSERT(textInputManagerV1);
        Q_ASSERT(textInputManagerV2);
        Q_ASSERT(textInputManagerV3);
    }

    void endGrab(wlr_input_method_keyboard_grab_v2 *kgv2)
    {
        if (!seat) {
            qCCritical(lcWlInputMethod) << "Failed to end input method keyboard grab - seat is already destroyed"
                                      << kgv2;
            return;
        }

        auto *kgHandle = kgv2;
        if (!kgHandle) {
            qCCritical(lcWlInputMethod) << "Failed to end input method keyboard grab - grab handle is invalid"
                                      << kgv2;
            return;
        }

        if (kgHandle->keyboard) {
            wlr_seat_keyboard_send_modifiers(seat->handle(), &kgHandle->keyboard->modifiers);
        }
        // Only end the grab if our grab is still the active one on the seat.
        // A popup grab may have silently replaced us (wlr_seat_keyboard_start_grab
        // unconditionally overwrites keyboard_state.grab).
        auto isStillActive = seat->handle()->keyboard_state.grab == &keyboardGrab;
        qCDebug(lcWlInputMethod) << "endGrab: isStillActive" << isStillActive << "grab ptr"
                                 << seat->handle()->keyboard_state.grab << "&keyboardGrab"
                                 << &keyboardGrab;
        if (isStillActive) {
            wlr_seat_keyboard_end_grab(seat->handle());
        }
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
            auto *virtualKeyboard = wlr_input_device_get_virtual_keyboard(keyboard->handle());
            // refer to:
            // https://github.com/swaywm/sway/blob/master/sway/input/keyboard.c#L391
            if (virtualKeyboard
                && virtualKeyboard->resource
                && kgHandle->resource
                && wl_resource_get_client(virtualKeyboard->resource)
                    == wl_resource_get_client(kgHandle->resource)) {
                return;
            }
            wlr_input_method_keyboard_grab_v2_set_keyboard(kgv2, wlr_keyboard_from_input_device(keyboard->handle()));
        } else {
            wlr_input_method_keyboard_grab_v2_set_keyboard(kgv2, nullptr);
        }
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

    wlr_seat_keyboard_grab keyboardGrab;
    wlr_keyboard_grab_interface grabInterface;
    GrabHandlerArg handlerArg;
    std::unique_ptr<WListenerOwner> keyboardGrabListenerOwner;

    QList<WTextInput *> textInputs;
    QList<WInputDevice *> virtualKeyboards;
    QList<WInputPopupSurface *> popupSurfaces;

};

WInputMethodHelper::WInputMethodHelper(WServer *server, WSeat *seat)
    : QObject(server)
    , WObject(*new WInputMethodHelperPrivate(server, seat, this))
{
    W_D(WInputMethodHelper);
    QObject::connect(d->seat, &WSeat::keyboardFocusSurfaceChanged, this, &WInputMethodHelper::resendKeyboardFocus);
    QObject::connect(d->seat, &WSeat::keyboardChanged, this, [d] {
        if (auto *activeKG = d->activeKeyboardGrab)
            d->setKeyboard(activeKG, d->seat->keyboard());
    });
    d->seat->listeners(this)->add(&d->seat->handle()->events.keyboard_grab_begin, this,
        &WInputMethodHelper::handleKeyboardGrabBegin);
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

WTextInput *WInputMethodHelper::focusedTextInput() const
{
    W_DC(WInputMethodHelper);
    auto focused = std::find_if(d->textInputs.begin(), d->textInputs.end(), [](WTextInput *ti) {
        return ti->focusedSurface() != nullptr;
    });
    return focused != d->textInputs.end() ? *focused : nullptr;
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
    if (d->activeInputMethod)
        d->activeInputMethod->listeners(this)->add(&im->handle()->events.destroy, this,
            &WInputMethodHelper::handleActiveIMDestroyed);
}

wlr_input_method_keyboard_grab_v2 *WInputMethodHelper::activeKeyboardGrab() const
{
    W_DC(WInputMethodHelper);
    return d->activeKeyboardGrab;
}

bool WInputMethodHelper::isActiveKeyboardGrabOwner() const
{
    W_DC(WInputMethodHelper);
    if (!d->activeKeyboardGrab)
        return false;
    return d->seat->handle()->keyboard_state.grab == &d->keyboardGrab;
}

const QList<WInputDevice *> &WInputMethodHelper::virtualKeyboards() const
{
    W_DC(WInputMethodHelper);
    return d->virtualKeyboards;
}

void WInputMethodHelper::handleNewIMV2(wlr_input_method_v2 *imv2)
{
    W_D(WInputMethodHelper);
    // Reject input methods from other seats before wrapping: the wrapper
    // attaches native listeners in its constructor, so an early return
    // afterwards would leak it (the listeners stay registered on the native
    // handle and assert on its destroy).
    auto *imSeat = WSeat::fromHandle(imv2->seat);
    if (!imSeat || d->seat->name() != imSeat->name())
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
    // Once input method is online, try to resend enter to textInput
    resendKeyboardFocus();
    // For text input v1, when after sendEnter, enabled signal will be emitted
}

void WInputMethodHelper::handleNewKGV2(wlr_input_method_keyboard_grab_v2 *kgv2)
{
    W_D(WInputMethodHelper);
    Q_ASSERT(d->seat);
    if (auto activeKG = activeKeyboardGrab()) {
        d->endGrab(activeKG);
    }
    d->activeKeyboardGrab = kgv2;
    d->setKeyboard(kgv2, d->seat->keyboard());
    d->grabInterface = *d->seat->handle()->keyboard_state.grab->interface;
    d->grabInterface.key = handleKey;
    d->grabInterface.modifiers = handleModifiers;
    d->keyboardGrab.seat = d->seat->handle();
    d->handlerArg.grab = kgv2;
    d->keyboardGrab.data = &d->handlerArg;
    d->keyboardGrab.interface = &d->grabInterface;
    wlr_seat_keyboard_start_grab(d->seat->handle(), &d->keyboardGrab);
    qCDebug(lcWlInputMethod) << "IME keyboard grab installed";
    d->keyboardGrabListenerOwner = std::make_unique<WListenerOwner>();
    auto *grabOwner = d->keyboardGrabListenerOwner.get();
    d->seat->listeners(grabOwner)->add(&kgv2->events.destroy, this, [this, d, kgv2, grabOwner] {
            qCDebug(lcWlInputMethod) << "IME keyboard grab before_destroy";
            Q_ASSERT(activeKeyboardGrab() == kgv2);
            d->endGrab(kgv2);
            d->activeKeyboardGrab = nullptr;
            d->handlerArg.grab = nullptr;
            d->seat->removeListeners(grabOwner);
            d->keyboardGrabListenerOwner.reset();
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

void WInputMethodHelper::handleKeyboardGrabBegin()
{
    W_D(WInputMethodHelper);
    // If another grab (popup, drag, etc.) silently replaced our keyboard grab,
    // notify all text inputs to leave so the IME can deactivate.
    // Our grab v2 object is still alive (endGrab only runs on before_destroy),
    // so activeKeyboardGrab is non-null, but seat->keyboard_state.grab no longer
    // points to our keyboardGrab.
    if (d->activeKeyboardGrab && d->seat->handle()->keyboard_state.grab != &d->keyboardGrab) {
        qCDebug(lcWlInputMethod) << "IME keyboard grab silently replaced, notifying leave";
        notifyLeave();
    }
}

void WInputMethodHelper::resendKeyboardFocus()
{
    W_D(WInputMethodHelper);
    qCInfo(lcWlInputMethod()) << "resend keyboard focus";
    auto focus = d->seat->keyboardFocusSurface();
    for (auto textInput : std::as_const(d->textInputs)) {
        if (textInput->focusedSurface() && textInput->focusedSurface() != focus)
            textInput->sendLeave();
        if (!focus)
            continue;
        qCDebug(lcWlInputMethod()) << "trying to send focus to" << textInput << "from client" << textInput->waylandClient();
        if (focus->waylandClient() == textInput->waylandClient()) {
            qCDebug(lcWlInputMethod) << "focus sent to" << textInput;
            if ((!textInput->seat() || textInput->seat() == d->seat)
                && textInput->focusedSurface() != focus) {
                textInput->sendEnter(focus);
            }
        }
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
    Q_ASSERT(ti);
    if (enabledTextInput() == ti) {
        // Should we consider the case when the same text input is disabled and then enabled at the same time.
        auto im = inputMethod();
        if (im) {
            im->sendDeactivate();
            im->sendDone();
        }
        setEnabledTextInput(nullptr);
    }
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
        if (ti->seat() && d->seat->name() == ti->seat()->name()) {
            connectToTI(ti);
            if (auto surface = d->seat->keyboardFocusSurface()) {
                ti->sendEnter(surface);
            }
        }
    });
    if (ti->seat() && d->seat->name() == ti->seat()->name()) {
        connectToTI(ti);
        if (auto *surface = d->seat->keyboardFocusSurface(); surface
            && surface->waylandClient() == ti->waylandClient()) {
            ti->sendEnter(surface);
        }
    }
}

void WInputMethodHelper::handleTIEnabled()
{
    WTextInput *ti = qobject_cast<WTextInput*>(sender());
    Q_ASSERT(ti);
    auto im = inputMethod();
    auto activeTI = enabledTextInput();
    if (activeTI == ti)
        return;
    if (activeTI) {
        if (im) {
            // If current active input method is not null, notify it to deactivate.
            im->sendDeactivate();
            im->sendDone();
        }
        // Notify last active text input to leave.
        activeTI->sendLeave();
    }
    setEnabledTextInput(ti);
    // Try to activate input method.
    if (im) {
        im->sendActivate();
        if (ti->features().testFlag(IME::F_SurroundingText)) {
            im->sendSurroundingText(ti->surroundingText(), ti->surroundingCursor(), ti->surroundingAnchor());
        }
        im->sendTextChangeCause(ti->textChangeCause());
        if (ti->features().testFlag(IME::F_ContentType)) {
            im->sendContentType(ti->contentHints().toInt(), ti->contentPurpose());
        }
        im->sendDone();
    }
}

void WInputMethodHelper::handleTIDisabled()
{
    WTextInput *ti = qobject_cast<WTextInput*>(sender());
    disableTI(ti);
}

void WInputMethodHelper::handleFocusedTICommitted()
{
    auto ti = enabledTextInput();
    Q_ASSERT(ti);
    if (!ti->focusedSurface()) {
        qCWarning(lcWlInputMethod) << "Discard commit to unfocused but not disabled text input.";
        return;
    }
    qCDebug(lcWlInputMethod) << "Focused text input" << ti << "committed."
                            << "Cursor rectangle:" << ti->cursorRect();
    auto im = inputMethod();
    if (im) {
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
    auto im = inputMethod();
    Q_ASSERT(im);
    auto ti = enabledTextInput();
    if (ti && ti->focusedSurface()) {
        ti->handleIMCommitted(im);
    }
}

void WInputMethodHelper::handleActiveIMDestroyed()
{
    auto im = inputMethod();
    Q_ASSERT(im);
    setInputMethod(nullptr);
    delete im;
    notifyLeave();
}

void WInputMethodHelper::notifyLeave()
{
    W_D(WInputMethodHelper);
    for (auto *ti : std::as_const(d->textInputs)) {
        if (ti->focusedSurface())
            ti->sendLeave();
    }
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
