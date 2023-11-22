// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodhelper_p.h"

#include "wquicktextinputv3_p.h"
#include "wquicktextinputv1_p.h"
#include "wquickinputmethodv2_p.h"
#include "wquickvirtualkeyboardv1_p.h"
#include "wquickseat_p.h"
#include "wseat.h"
#include "wsurface.h"
#include "wxdgsurface.h"

#include <qwcompositor.h>
#include <qwtextinputv3.h>
#include <qwvirtualkeyboardv1.h>
#include <qwseat.h>

#include <QLoggingCategory>
#include <QQmlInfo>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
#include <wlr/types/wlr_text_input_v3.h>
#define delete delete_c
#include <wlr/types/wlr_input_method_v2.h>
#undef delete
#include <wlr/types/wlr_seat.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_LOGGING_CATEGORY(qLcInputMethod, "waylib.server.im", QtInfoMsg)
// TODO: Provide customization and a more sensible policy for placing input popup.
static constexpr int InputPopupTopMargin = 2;

struct GrabHandlerArg {
    const WInputMethodHelper *const helper;
    const WQuickInputMethodKeyboardGrabV2 * grab;
};

static inline void handleKey(struct wlr_seat_keyboard_grab *grab, uint32_t time_msec, uint32_t key, uint32_t state)
{
    auto arg = reinterpret_cast<GrabHandlerArg*>(grab->data);
    for (auto vk: arg->helper->virtualKeyboardManagerV1()->virtualKeyboards()) {
        if (wlr_keyboard_from_input_device(vk->keyboard()->handle()->handle()) == grab->seat->keyboard_state.keyboard) {
            grab->seat->keyboard_state.default_grab->interface->key(grab, time_msec, key, state);
            return;
        }
    }
    arg->grab->handle()->sendKey(time_msec, key, state);
}

static inline void handleModifiers(struct wlr_seat_keyboard_grab *grab, const struct wlr_keyboard_modifiers *modifiers)
{
    auto arg = reinterpret_cast<GrabHandlerArg*>(grab->data);
    for (auto vk: arg->helper->virtualKeyboardManagerV1()->virtualKeyboards()) {
        if (wlr_keyboard_from_input_device(vk->keyboard()->handle()->handle()) == grab->seat->keyboard_state.keyboard) {
            grab->seat->keyboard_state.default_grab->interface->modifiers(grab, modifiers);
            return;
        }
    }
    arg->grab->handle()->sendModifiers(const_cast<wlr_keyboard_modifiers*>(modifiers));
}

class WInputMethodHelperPrivate : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WInputMethodHelper)
public:
    explicit WInputMethodHelperPrivate(WInputMethodHelper *qq)
        : WObjectPrivate(qq)
        , handlerArg({.helper = qq})
    {}

    WQuickSeat *seat;
    WQuickInputMethodManagerV2 *inputMethodManagerV2;
    WQuickTextInputManagerV1 *textInputManagerV1;
    WQuickTextInputManagerV3 *textInputManagerV3;
    WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1;

    WQuickTextInputV1 *activeTextInputV1;
    WQuickTextInputV3 *activeTextInputV3;
    WQuickInputMethodV2 *inputMethodV2;
    QList<WInputPopupV2 *> popupSurfaces;
    QRect cursorRect;
    QQuickItem *activeFocusItem;

    wlr_seat_keyboard_grab keyboardGrab;
    wlr_keyboard_grab_interface grabInterface;
    GrabHandlerArg handlerArg;
};

WInputMethodHelper::WInputMethodHelper(QObject *parent)
    : QObject(parent)
    , WObject(*new WInputMethodHelperPrivate(this))
{ }

WQuickSeat *WInputMethodHelper::seat() const
{
    W_DC(WInputMethodHelper);
    return d->seat;
}

WQuickTextInputManagerV3 *WInputMethodHelper::textInputManagerV3() const
{
    W_DC(WInputMethodHelper);
    return d->textInputManagerV3;
}

void WInputMethodHelper::setTextInputManagerV3(WQuickTextInputManagerV3 *textInputManagerV3)
{
    W_D(WInputMethodHelper);
    if (d->textInputManagerV3) {
        qmlWarning(this) << "Trying to set text input manager v3 for input method helper twice. Ignore this request";
        return;
    }
    d->textInputManagerV3 = textInputManagerV3;
    if (textInputManagerV3) {
        connect(d->textInputManagerV3, &WQuickTextInputManagerV3::newTextInput, this, &WInputMethodHelper::onNewTextInputV3);
    }
}

WQuickTextInputManagerV1 *WInputMethodHelper::textInputManagerV1() const
{
    W_DC(WInputMethodHelper);
    return d->textInputManagerV1;
}

void WInputMethodHelper::setTextInputManagerV1(WQuickTextInputManagerV1 *textInputManagerV1)
{
    W_D(WInputMethodHelper);
    if (d->textInputManagerV1) {
        qmlWarning(this) << "Trying to set text input manager v1 for input method helper twice. Ignore this request.";
        return;
    }
    d->textInputManagerV1 = textInputManagerV1;
    if (textInputManagerV1) {
        connect(d->textInputManagerV1, &WQuickTextInputManagerV1::newTextInput, this, &WInputMethodHelper::onNewTextInputV1);
    }
}

WQuickInputMethodManagerV2 *WInputMethodHelper::inputMethodManagerV2() const
{
    W_DC(WInputMethodHelper);
    return d->inputMethodManagerV2;
}

void WInputMethodHelper::setInputMethodManagerV2(WQuickInputMethodManagerV2 *inputMethodManagerV2)
{
    W_D(WInputMethodHelper);
    if (d->inputMethodManagerV2) {
        qmlWarning(this) << "Trying to set input method manager v2 for input method helper twice. Ignore this request.";
        return;
    }
    d->inputMethodManagerV2 = inputMethodManagerV2;
    if (inputMethodManagerV2) {
        connect(d->inputMethodManagerV2, &WQuickInputMethodManagerV2::newInputMethod, this, &WInputMethodHelper::onNewInputMethodV2);
    }
}

WQuickVirtualKeyboardManagerV1 *WInputMethodHelper::virtualKeyboardManagerV1() const
{
    W_DC(WInputMethodHelper);
    return d->virtualKeyboardManagerV1;
}

void WInputMethodHelper::setVirtualKeyboardManagerV1(
    WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1)
{
    W_D(WInputMethodHelper);
    if (d->virtualKeyboardManagerV1) {
        qmlWarning(this) << "Trying to set virtual keyboard manager v1 for input method helper twice. Ignore this request.";
        return;
    }
    d->virtualKeyboardManagerV1 = virtualKeyboardManagerV1;
    if (virtualKeyboardManagerV1) {
        connect(d->virtualKeyboardManagerV1, &WQuickVirtualKeyboardManagerV1::newVirtualKeyboard, this, &WInputMethodHelper::onNewVirtualKeyboardV1);
    }
}

WQuickTextInputV1 *WInputMethodHelper::activeTextInputV1() const
{
    return d_func()->activeTextInputV1;
}

void WInputMethodHelper::setActiveTextInputV1(WQuickTextInputV1 *newActiveTextInputV1)
{
    W_D(WInputMethodHelper);
    if (newActiveTextInputV1 == d->activeTextInputV1)
        return;
    d->activeTextInputV1 = newActiveTextInputV1;
    Q_EMIT this->activeTextInputV1Changed(d->activeTextInputV1);
}

WQuickTextInputV3 *WInputMethodHelper::activeTextInputV3() const
{
    W_DC(WInputMethodHelper);
    return d->activeTextInputV3;
}

void WInputMethodHelper::setActiveTextInputV3(WQuickTextInputV3 *newActiveTextInputV3)
{
    W_D(WInputMethodHelper);
    if (d->activeTextInputV3 != newActiveTextInputV3) {
        d->activeTextInputV3 = newActiveTextInputV3;
        Q_EMIT this->activeTextInputV3Changed(newActiveTextInputV3);
    }
}

WQuickInputMethodV2 *WInputMethodHelper::inputMethodV2() const
{
    W_DC(WInputMethodHelper);
    return d->inputMethodV2;
}

void WInputMethodHelper::setInputMethodV2(WQuickInputMethodV2 *newInputMethodV2)
{
    W_D(WInputMethodHelper);
    if (d->inputMethodV2 != newInputMethodV2) {
        d->inputMethodV2 = newInputMethodV2;
        Q_EMIT this->inputMethodV2Changed(newInputMethodV2);
    }
}

QRect WInputMethodHelper::cursorRect() const
{
    return d_func()->cursorRect;
}

void WInputMethodHelper::setCursorRect(const QRect &rect)
{
    W_D(WInputMethodHelper);
    if (d->cursorRect == rect)
        return;
    d->cursorRect = rect;
    Q_EMIT cursorRectChanged();
}

QQuickItem *WInputMethodHelper::activeFocusItem() const
{
    return d_func()->activeFocusItem;
}

void WInputMethodHelper::setActiveFocusItem(QQuickItem *item)
{
    W_D(WInputMethodHelper);
    if (d->activeFocusItem == item)
        return;
    d->activeFocusItem = item;
    Q_EMIT this->activeFocusItemChanged();
}

void WInputMethodHelper::onNewInputMethodV2(WQuickInputMethodV2 *newInputMethod)
{
    if (wseat()->name() != newInputMethod->seat()->name())
        return;
    if (inputMethodV2()) {
        qCWarning(qLcInputMethod) << "Ignore second creation of input on the same seat.";
        newInputMethod->sendUnavailable();
    }
    setInputMethodV2(newInputMethod);

    // If there is a active text input v1, just activate the input method
    if (activeTextInputV1()) {
        newInputMethod->sendActivate();
        newInputMethod->sendDone();
    } else {
        // Once input method is online, try to resend enter to textInput
        resendKeyboardFocus();
    }

    connect(newInputMethod->handle(), &QWInputMethodV2::beforeDestroy, this, [this]{
        setInputMethodV2(nullptr);
        notifyKeyboardFocusLost();
    });
    connect(newInputMethod, &WQuickInputMethodV2::commit, this, &WInputMethodHelper::onInputMethodV2Committed);
    connect(newInputMethod, &WQuickInputMethodV2::newKeyboardGrab, this, &WInputMethodHelper::onNewKeyboardGrab);
    connect(newInputMethod, &WQuickInputMethodV2::newPopupSurface, this, &WInputMethodHelper::onInputPopupSurface);
}

void WInputMethodHelper::onNewTextInputV1(WQuickTextInputV1 *newTextInputV1)
{
    // Actually when textInputV1 is not activated, just created, we don't need to do anything, cause it is not associated with seat
    // Unlike text input v3, we should listen on each text input v1 object cause it might be activated on this seat at any time
    connect(newTextInputV1, &WQuickTextInputV1::seatChanged, this, [this, newTextInputV1](){
        auto st = newTextInputV1->seat();
        if (st && this->wseat()->name() == st->name()) {
            if (activeTextInputV1()) {
                // Let previous active text input leave
                activeTextInputV1()->sendLeave();
            }
            if (activeTextInputV3()) {
                activeTextInputV3()->sendLeave();
            }
            newTextInputV1->sendEnter(newTextInputV1->focusedSurface());
            setActiveTextInputV1(newTextInputV1);
            if (inputMethodV2()) {
                inputMethodV2()->sendActivate();
                inputMethodV2()->sendDone();
            }
        } else {
            onTextInputV1Deactivated(newTextInputV1);
        }
    });
    connect(newTextInputV1, &WQuickTextInputV1::commit, this, [this, newTextInputV1] (){
        auto im = inputMethodV2();
        if (!im)
            return;
        im->sendContentType(newTextInputV1->contentHint(), newTextInputV1->contentPurpose());
        im->sendSurroundingText(newTextInputV1->surroundingText(), newTextInputV1->surroundingTextCursor(), newTextInputV1->surroundingTextAnchor());
        setCursorRect(newTextInputV1->cursorRectangle());
        updateAllPopupSurfaces(newTextInputV1->cursorRectangle());
        im->sendDone();
    });
    connect(newTextInputV1, &WQuickTextInputV1::beforeDestroy, this, [this, newTextInputV1] (){
        onTextInputV1Deactivated(newTextInputV1);
    });
}

void WInputMethodHelper::onTextInputV1Deactivated(WQuickTextInputV1 *textInputV1)
{
    if (activeTextInputV1() && activeTextInputV1() == textInputV1) {
        activeTextInputV1()->sendLeave();
        setActiveTextInputV1(nullptr);
        if (inputMethodV2()) {
            inputMethodV2()->sendDeactivate();
            inputMethodV2()->sendDone();
        }
    }
}

void WInputMethodHelper::onNewTextInputV3(WQuickTextInputV3 *newTextInput)
{
    W_D(WInputMethodHelper);
    // Only cares about current seat's text input
    if (wseat()->name() == newTextInput->seat()->name()) {
        connect(newTextInput, &WQuickTextInputV3::enable, this, [this, newTextInput]() { Q_EMIT this->onTextInputV3Enabled(newTextInput); });
        connect(newTextInput, &WQuickTextInputV3::disable, this, [this, newTextInput]() { Q_EMIT this->onTextInputV3Disabled(newTextInput); });
        connect(newTextInput, &WQuickTextInputV3::commit, this, [this, newTextInput]() { Q_EMIT this->onTextInputV3Committed(newTextInput); });
        connect(newTextInput->handle(), &QWTextInputV3::beforeDestroy, this, [this, newTextInput]() { Q_EMIT this->onTextInputV3Destroyed(newTextInput); });
    }
}

void WInputMethodHelper::onNewVirtualKeyboardV1(WQuickVirtualKeyboardV1 *newVirtualKeyboard)
{
    wseat()->attachInputDevice(newVirtualKeyboard->keyboard());
    connect(newVirtualKeyboard->handle(), &QWVirtualKeyboardV1::beforeDestroy, this, [this, newVirtualKeyboard] () {
        wseat()->detachInputDevice(newVirtualKeyboard->keyboard());
    });
}

void WInputMethodHelper::onTextInputV3Enabled(WQuickTextInputV3 *enabledTextInputV3)
{
    if (activeTextInputV3()) {
        // Assume that previous active text input has been disabled
        onTextInputV3Disabled(activeTextInputV3());
    }
    if (activeTextInputV1()) {
        onTextInputV1Deactivated(activeTextInputV1());
    }
    setActiveTextInputV3(enabledTextInputV3);
    auto im = inputMethodV2();
    if (im) {
        im->sendActivate();
        sendInputMethodV2State(enabledTextInputV3);
    }
}

void WInputMethodHelper::onTextInputV3Disabled(WQuickTextInputV3 *disabledTextInputV3)
{
    if (activeTextInputV3() == disabledTextInputV3) {
        // Should we consider the case when the same text input is disabled and then enabled at the same time.
        setActiveTextInputV3(nullptr);
        auto im = inputMethodV2();
        if (im) {
            im->sendDeactivate();
            sendInputMethodV2State(disabledTextInputV3);
        }
    }
}

void WInputMethodHelper::onTextInputV3Committed(WQuickTextInputV3 *textInputV3)
{
    auto im = inputMethodV2();
    if (!im)
        return;
    const WTextInputV3State *const current = textInputV3->state();
    sendInputMethodV2State(textInputV3);
}

void WInputMethodHelper::onTextInputV3Destroyed(WQuickTextInputV3 *destroyedTextInputV3)
{
    onTextInputV3Disabled(destroyedTextInputV3);
}

void WInputMethodHelper::onInputMethodV2Committed()
{
    W_D(WInputMethodHelper);
    auto inputMethod = qobject_cast<WQuickInputMethodV2 *>(sender());
    Q_ASSERT(inputMethod);
    const WInputMethodV2State *const current = inputMethod->state();
    auto textInputV3 = activeTextInputV3();
    if (textInputV3) {
        if (!current->preeditStringText().isEmpty()) {
            textInputV3->sendPreeditString(current->preeditStringText(), current->preeditStringCursorBegin(), current->preeditStringCursorEnd());
        }
        if (!current->commitString().isEmpty()) {
            textInputV3->sendCommitString(current->commitString());
        }
        if (current->deleteSurroundingBeforeLength() || current->deleteSurroundingAfterLength()) {
            textInputV3->sendDeleteSurroundingText(current->deleteSurroundingBeforeLength(), current->deleteSurroundingAfterLength());
        }
        textInputV3->sendDone();
    }
    auto textInputV1 = activeTextInputV1();
    if (textInputV1) {
        if (current->deleteSurroundingBeforeLength() || current->deleteSurroundingAfterLength()) {
            textInputV1->sendDeleteSurroundingText(0, textInputV1->surroundingText().length() - current->deleteSurroundingBeforeLength() - current->deleteSurroundingAfterLength());
        }
        textInputV1->sendCommitString(current->commitString());
        if (!current->preeditStringText().isEmpty()) {
            textInputV1->sendPreeditStyling(0, current->preeditStringCursorEnd() - current->preeditStringCursorBegin(), WQuickTextInputV1::PS_Active);
            textInputV1->sendPreeditCursor(current->preeditStringCursorEnd() - current->preeditStringCursorBegin());
            textInputV1->sendPreeditString(current->preeditStringText(), current->commitString());
        }
    }
}

void WInputMethodHelper::resendKeyboardFocus()
{
    notifyKeyboardFocusLost();
    sendKeyboardFocus();
}

void WInputMethodHelper::sendInputMethodV2State(WQuickTextInputV3 *textInput)
{
    auto im = inputMethodV2();
    if (!im) {
        qCWarning(qLcInputMethod) << "Sending input method v2 state but input method is gone.";
        return;
    }
    auto current = textInput->state();
    if (current->features() & WTextInputV3State::SurroundingText) {
        im->sendSurroundingText(current->surroundingText(), current->surroundingCursor(), current->surroundingAnchor());
    }
    im->sendTextChangeCause(current->textChangeCause());
    if (current->features() & WTextInputV3State::ContentType) {
        im->sendContentType(current->contentHint(), current->contentPurpose());
    }
    if (current->features() & WTextInputV3State::CursorRect) {
        setCursorRect(current->cursorRect());
        updateAllPopupSurfaces(current->cursorRect());
    }
    im->sendDone();
}

void WInputMethodHelper::onNewKeyboardGrab(WQuickInputMethodKeyboardGrabV2 *keyboardGrab)
{
    W_D(WInputMethodHelper);
    keyboardGrab->setKeyboard(wseat()->keyboard());
    connect(wseat(), &WSeat::keyboardChanged, keyboardGrab, [this, keyboardGrab](){
        keyboardGrab->setKeyboard(wseat()->keyboard());
    });

    d->grabInterface = *wseat()->nativeHandle()->keyboard_state.grab->interface;
    d->grabInterface.key = handleKey;
    d->grabInterface.modifiers = handleModifiers;
    d->keyboardGrab.seat = wseat()->nativeHandle();
    d->handlerArg.grab = keyboardGrab;
    d->keyboardGrab.data = &d->handlerArg;
    d->keyboardGrab.interface = &d->grabInterface;
    wseat()->handle()->keyboardStartGrab(&d->keyboardGrab);
    connect(keyboardGrab->handle(), &QWInputMethodKeyboardGrabV2::beforeDestroy, this, [this, keyboardGrab](){
        if (keyboardGrab->keyboard()) {
            wseat()->handle()->keyboardSendModifiers(&keyboardGrab->nativeHandle()->keyboard->modifiers);
        }
        wseat()->handle()->keyboardEndGrab();
    });
}

void WInputMethodHelper::onInputPopupSurface(WQuickInputPopupSurfaceV2 *popupSurface)
{
    W_D(WInputMethodHelper);

    auto createPopupSurface = [this, d] (WSurface *focus, QRect cursorRect, WQuickInputPopupSurfaceV2 *popupSurface){
        auto surface = new WInputPopupV2(popupSurface, focus, this);
        d->popupSurfaces.append(surface);
        Q_EMIT this->inputPopupSurfaceV2Added(surface);
        updatePopupSurface(surface, cursorRect);
        connect(popupSurface->handle(), &QWInputPopupSurfaceV2::beforeDestroy, this, [this, d, surface](){
            Q_EMIT this->inputPopupSurfaceV2Removed(surface);
            d->popupSurfaces.removeAll(surface);
            surface->deleteLater();
        });
    };

    if (auto ti = activeTextInputV1()) {
        createPopupSurface(ti->focusedSurface(), ti->cursorRectangle(), popupSurface);
    }

    if (auto ti = activeTextInputV3()) {
        createPopupSurface(ti->focusedSurface(), ti->state()->cursorRect(), popupSurface);
    }
}

void WInputMethodHelper::notifyKeyboardFocusLost()
{
    W_D(WInputMethodHelper);
    for (auto textInput : d->textInputManagerV1->textInputs()) {
        if (textInput->focusedSurface()) {
            textInput->sendLeave();
        }
    }
    for (auto textInput : d->textInputManagerV3->textInputs()) {
        if (textInput->focusedSurface()) {
            textInput->sendLeave();
        }
    }
}

void WInputMethodHelper::updateAllPopupSurfaces(QRect cursorRect)
{
    for (auto popup : d_func()->popupSurfaces) {
        updatePopupSurface(popup, cursorRect);
    }
}

void WInputMethodHelper::updatePopupSurface(WInputPopupV2 *popup, QRect cursorRect)
{
    popup->handle()->sendTextInputRectangle(cursorRect);
}

void WInputMethodHelper::sendKeyboardFocus()
{
    auto focus = seat()->keyboardFocus();
    if (!focus)
        return;
    for (auto textInput : d_func()->textInputManagerV3->textInputs()) {
        if (wl_resource_get_client(focus->handle()->handle()->resource) == textInput->waylandClient()) {
            textInput->sendEnter(focus);
        }
    }
}

void WInputMethodHelper::setSeat(WQuickSeat *newSeat)
{
    W_D(WInputMethodHelper);
    if (d->seat) {
        qmlWarning(this) << "Trying to set seat for input method helper twice. Ignore this request.";
        return;
    }
    d->seat = newSeat;
    if (newSeat) {
        connect(d->seat, &WQuickSeat::keyboardFocusChanged, this, &WInputMethodHelper::resendKeyboardFocus);
    }
}

WSeat *WInputMethodHelper::wseat() const
{
    return seat()->seat();
}
WAYLIB_SERVER_END_NAMESPACE
