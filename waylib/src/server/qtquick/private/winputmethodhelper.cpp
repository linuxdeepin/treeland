// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodhelper_p.h"
#include "wquicktextinputv3_p.h"
#include "wquicktextinputv1_p.h"
#include "winputmethodcommon_p.h"
#include "wquickinputmethodv2_p.h"
#include "wquickvirtualkeyboardv1_p.h"
#include "wquickseat_p.h"
#include "wseat.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"

#include <qwcompositor.h>
#include <qwinputmethodv2.h>
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

struct GrabHandlerArg {
    const WInputMethodHelper *const helper;
    WQuickInputMethodKeyboardGrabV2 * grab;
};

static inline void handleKey(struct wlr_seat_keyboard_grab *grab, uint32_t time_msec, uint32_t key, uint32_t state)
{
    auto arg = reinterpret_cast<GrabHandlerArg*>(grab->data);
    for (auto vk: arg->helper->virtualKeyboards()) {
        if (wlr_keyboard_from_input_device(vk->keyboard()->handle()->handle()) == grab->seat->keyboard_state.keyboard) {
            grab->seat->keyboard_state.default_grab->interface->key(grab, time_msec, key, state);
            return;
        }
    }
    arg->grab->sendKey(time_msec, Qt::Key(key), state);
}

static inline void handleModifiers(struct wlr_seat_keyboard_grab *grab, const struct wlr_keyboard_modifiers *modifiers)
{
    auto arg = reinterpret_cast<GrabHandlerArg*>(grab->data);
    for (auto vk: arg->helper->virtualKeyboards()) {
        if (wlr_keyboard_from_input_device(vk->keyboard()->handle()->handle()) == grab->seat->keyboard_state.keyboard) {
            grab->seat->keyboard_state.default_grab->interface->modifiers(grab, modifiers);
            return;
        }
    }
    arg->grab->sendModifiers(modifiers->depressed, modifiers->latched, modifiers->locked, modifiers->group);
}

class WInputMethodHelperPrivate : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WInputMethodHelper)
public:
    explicit WInputMethodHelperPrivate(WInputMethodHelper *qq)
        : WObjectPrivate(qq)
        , handlerArg({.helper = qq})
    {}

    WQuickSeat *seat { nullptr };
    WQuickInputMethodManagerV2 *inputMethodManagerV2 { nullptr };
    WQuickTextInputManagerV1 *textInputManagerV1 { nullptr };
    WQuickTextInputManagerV3 *textInputManagerV3 { nullptr };
    WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1 { nullptr };
    QQuickItem *activeFocusItem { nullptr };
    WQuickTextInput *activeTextInput { nullptr };
    WQuickInputMethodV2 *activeInputMethod { nullptr };
    WQuickInputMethodKeyboardGrabV2 *activeKeyboardGrab {nullptr};

    wlr_seat_keyboard_grab keyboardGrab;
    wlr_keyboard_grab_interface grabInterface;
    GrabHandlerArg handlerArg;
    QRect cursorRect;

    QList<WQuickTextInput *> textInputs;
    QList<WQuickVirtualKeyboardV1 *> virtualKeyboards;
    QList<WInputPopupV2 *> popupSurfaces;
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
    if (d->textInputManagerV3 == textInputManagerV3)
        return;
    if (d->textInputManagerV3) {
        d->textInputManagerV3->disconnect(this);
    }
    d->textInputManagerV3 = textInputManagerV3;
    if (textInputManagerV3) {
        connect(d->textInputManagerV3, &WQuickTextInputManagerV3::newTextInput, this, &WInputMethodHelper::handleNewTI)  ;
    }
    Q_EMIT textInputManagerV3Changed();
}

WQuickTextInputManagerV1 *WInputMethodHelper::textInputManagerV1() const
{
    W_DC(WInputMethodHelper);
    return d->textInputManagerV1;
}

void WInputMethodHelper::setTextInputManagerV1(WQuickTextInputManagerV1 *textInputManagerV1)
{
    W_D(WInputMethodHelper);
    if (d->textInputManagerV1 == textInputManagerV1)
        return;
    if (d->textInputManagerV1) {
        d->textInputManagerV1->disconnect(this);
    }
    d->textInputManagerV1 = textInputManagerV1;
    if (textInputManagerV1) {
        connect(d->textInputManagerV1, &WQuickTextInputManagerV1::newTextInput, this, &WInputMethodHelper::handleNewTI);
    }
    Q_EMIT textInputManagerV1Changed();
}

WQuickInputMethodManagerV2 *WInputMethodHelper::inputMethodManagerV2() const
{
    W_DC(WInputMethodHelper);
    return d->inputMethodManagerV2;
}

void WInputMethodHelper::setInputMethodManagerV2(WQuickInputMethodManagerV2 *inputMethodManagerV2)
{
    W_D(WInputMethodHelper);
    if (d->inputMethodManagerV2 == inputMethodManagerV2)
        return;
    if (d->inputMethodManagerV2) {
        d->inputMethodManagerV2->disconnect(this);
    }
    d->inputMethodManagerV2 = inputMethodManagerV2;
    if (inputMethodManagerV2) {
        connect(d->inputMethodManagerV2, &WQuickInputMethodManagerV2::newInputMethod, this, &WInputMethodHelper::handleNewIMV2);
    }
    Q_EMIT inputMethodManagerV2Changed();
}

WQuickVirtualKeyboardManagerV1 *WInputMethodHelper::virtualKeyboardManagerV1() const
{
    W_DC(WInputMethodHelper);
    return d->virtualKeyboardManagerV1;
}

void WInputMethodHelper::setVirtualKeyboardManagerV1(WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1)
{
    W_D(WInputMethodHelper);
    if (d->virtualKeyboardManagerV1 == virtualKeyboardManagerV1)
        return;
    if (d->virtualKeyboardManagerV1) {
        d->virtualKeyboardManagerV1->disconnect(this);
    }
    d->virtualKeyboardManagerV1 = virtualKeyboardManagerV1;
    if (virtualKeyboardManagerV1) {
        connect(d->virtualKeyboardManagerV1, &WQuickVirtualKeyboardManagerV1::newVirtualKeyboard, this, &WInputMethodHelper::handleNewVKV1);
    }
    Q_EMIT virtualKeyboardManagerV1Changed();
}

WQuickTextInput *WInputMethodHelper::focusedTextInput() const
{
    W_DC(WInputMethodHelper);
    return d->activeTextInput;
}

void WInputMethodHelper::setFocusedTextInput(WQuickTextInput *ti)
{
    W_D(WInputMethodHelper);
    if (d->activeTextInput == ti)
        return;
    if (d->activeTextInput) {
        disconnect(d->activeTextInput, &WQuickTextInput::committed, this, &WInputMethodHelper::handleFocusedTICommitted);
    }
    d->activeTextInput = ti;
    if (ti) {
        setCursorRect(ti->cursorRect());
        updateAllPopupSurfaces(ti->cursorRect()); // Note: if this is necessary
        connect(ti, &WQuickTextInput::committed, this, &WInputMethodHelper::handleFocusedTICommitted, Qt::UniqueConnection);
    }
    Q_EMIT activeTextInputChanged();
}

WQuickInputMethodV2 *WInputMethodHelper::inputMethod() const
{
    W_DC(WInputMethodHelper);
    return d->activeInputMethod;
}

void WInputMethodHelper::setInputMethod(WQuickInputMethodV2 *im)
{
    W_D(WInputMethodHelper);
    if (d->activeInputMethod == im)
        return;
    if (d->activeInputMethod)
        d->activeInputMethod->safeDisconnect(this);
    d->activeInputMethod = im;
    Q_EMIT activeInputMethodChanged();
}

QRect WInputMethodHelper::cursorRect() const
{
    W_DC(WInputMethodHelper);
    return d->cursorRect;
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
    W_DC(WInputMethodHelper);
    return d->activeFocusItem;
}

void WInputMethodHelper::setActiveFocusItem(QQuickItem *item)
{
    W_D(WInputMethodHelper);
    if (d->activeFocusItem == item)
        return;
    d->activeFocusItem = item;
    Q_EMIT this->activeFocusItemChanged();
}

WQuickInputMethodKeyboardGrabV2 *WInputMethodHelper::activeKeyboardGrab() const
{
    W_DC(WInputMethodHelper);
    return d->activeKeyboardGrab;
}

QList<WQuickVirtualKeyboardV1 *> WInputMethodHelper::virtualKeyboards() const
{
    W_DC(WInputMethodHelper);
    return d->virtualKeyboards;
}

void WInputMethodHelper::handleNewTI(QObject *ti)
{
    if (auto ti1 = qobject_cast<WTextInputV1*>(ti)) {
        tryAddTextInput(new WQuickTextInputV1(ti1, this));
    } else if (auto ti3 = qobject_cast<QWTextInputV3 *>(ti)) {
        tryAddTextInput(new WQuickTextInputV3(ti3, this));
    } else {
        Q_UNREACHABLE();
    }
}

void WInputMethodHelper::handleNewIMV2(QWInputMethodV2 *imv2)
{
    auto wimv2 = new WQuickInputMethodV2(imv2, this);
    if (wseat()->name() != wimv2->seat()->name())
        return;
    if (inputMethod()) {
        qCWarning(qLcInputMethod) << "Ignore second creation of input on the same seat.";
        wimv2->sendUnavailable();
        return;
    }
    setInputMethod(wimv2);
    connect(wimv2, &WQuickInputMethodV2::committed, this, &WInputMethodHelper::handleIMCommitted);
    connect(wimv2, &WQuickInputMethodV2::newKeyboardGrab, this, &WInputMethodHelper::handleNewKGV2);
    connect(wimv2, &WQuickInputMethodV2::newPopupSurface, this, &WInputMethodHelper::handleNewIPSV2);
    // Once input method is online, try to resend enter to textInput
    resendKeyboardFocus();
    // For text input v1, when after sendEnter, enabled signal will be emitted
    wimv2->safeConnect(&QWInputMethodV2::beforeDestroy, this, [this, wimv2]{
        if (inputMethod() == wimv2) {
            setInputMethod(nullptr);
        }
        wimv2->safeDeleteLater();
        notifyLeave();
    });

}

void WInputMethodHelper::handleNewKGV2(QWInputMethodKeyboardGrabV2 *kgv2)
{
    W_D(WInputMethodHelper);
    auto endGrab = [this](WQuickInputMethodKeyboardGrabV2 *wkgv2) {
        if (wkgv2->keyboard()) {
            wseat()->handle()->keyboardSendModifiers(&wkgv2->handle()->handle()->keyboard->modifiers);
        }
        wseat()->handle()->keyboardEndGrab();
    };
    if (auto activeKG = activeKeyboardGrab()) {
        endGrab(activeKG);
    }
    auto wkgv2 = new WQuickInputMethodKeyboardGrabV2(kgv2, this);
    d->activeKeyboardGrab = wkgv2;
    wkgv2->setKeyboard(wseat()->keyboard());
    connect(wseat(), &WSeat::keyboardChanged, wkgv2, [this, wkgv2](){
        wkgv2->setKeyboard(wseat()->keyboard());
    });
    d->grabInterface = *wseat()->nativeHandle()->keyboard_state.grab->interface;
    d->grabInterface.key = handleKey;
    d->grabInterface.modifiers = handleModifiers;
    d->keyboardGrab.seat = wseat()->nativeHandle();
    d->handlerArg.grab = wkgv2;
    d->keyboardGrab.data = &d->handlerArg;
    d->keyboardGrab.interface = &d->grabInterface;
    wseat()->handle()->keyboardStartGrab(&d->keyboardGrab);
    connect(kgv2, &QWInputMethodKeyboardGrabV2::beforeDestroy, wkgv2, [this, d, endGrab, wkgv2] {
        if (activeKeyboardGrab() == wkgv2) {
            endGrab(wkgv2);
            d->activeKeyboardGrab = nullptr;
        }
        wkgv2->deleteLater();
    });
}

void WInputMethodHelper::handleNewIPSV2(QWInputPopupSurfaceV2 *ipsv2)
{
    W_D(WInputMethodHelper);

    auto createPopupSurface = [this, d] (WSurface *focus, QRect cursorRect, QWInputPopupSurfaceV2 *popupSurface){
        auto surface = new WInputPopupV2(popupSurface, focus, this);
        d->popupSurfaces.append(surface);
        Q_EMIT inputPopupSurfaceV2Added(surface);
        updatePopupSurface(surface, cursorRect);
        surface->safeConnect(&QWInputPopupSurfaceV2::beforeDestroy, this, [this, d, surface](){
            d->popupSurfaces.removeAll(surface);
            Q_EMIT inputPopupSurfaceV2Removed(surface);
            surface->safeDeleteLater();
        });
    };

    if (auto ti = focusedTextInput()) {
        createPopupSurface(ti->focusedSurface(), ti->cursorRect(), ipsv2);
    }
}

void WInputMethodHelper::handleNewVKV1(QWVirtualKeyboardV1 *vkv1)
{
    W_D(WInputMethodHelper);
    auto wvkv1 = new WQuickVirtualKeyboardV1(vkv1, this);
    d->virtualKeyboards.append(wvkv1);
    wseat()->attachInputDevice(wvkv1->keyboard());
    auto result = connect(vkv1, &QWVirtualKeyboardV1::beforeDestroy, wvkv1, [d, this, wvkv1] () {
        wseat()->detachInputDevice(wvkv1->keyboard());
        d->virtualKeyboards.removeOne(wvkv1);
        wvkv1->deleteLater();
    });
    qDebug() << result;
}

void WInputMethodHelper::resendKeyboardFocus()
{
    W_D(WInputMethodHelper);
    notifyLeave();
    auto focus = seat()->keyboardFocus();
    if (!focus)
        return;
    for (auto textInput : d->textInputs) {
        if (focus->waylandClient() == textInput->waylandClient()) {
            textInput->sendEnter(focus);
        }
    }
}

QString WInputMethodHelper::seatName() const
{
    return wseat()->name();
}

void WInputMethodHelper::connectToTI(WQuickTextInput *ti)
{
    connect(ti, &WQuickTextInput::enabled, this, &WInputMethodHelper::handleTIEnabled, Qt::UniqueConnection);
    connect(ti, &WQuickTextInput::disabled, this, &WInputMethodHelper::handleTIDisabled, Qt::UniqueConnection);
    connect(ti, &WQuickTextInput::requestLeave, ti, &WQuickTextInput::sendLeave, Qt::UniqueConnection);
}

void WInputMethodHelper::disableTI(WQuickTextInput *ti)
{
    Q_ASSERT(ti);
    W_D(WInputMethodHelper);
    if (focusedTextInput() == ti) {
        // Should we consider the case when the same text input is disabled and then enabled at the same time.
        auto im = inputMethod();
        if (im) {
            im->sendDeactivate();
            im->sendDone();
        }
        setFocusedTextInput(nullptr);
    }
}

void WInputMethodHelper::tryAddTextInput(WQuickTextInput *ti)
{
    W_D(WInputMethodHelper);
    if (d->textInputs.contains(ti))
        return;
    d->textInputs.append(ti);
    connect(ti, &WQuickTextInput::entityAboutToDestroy, this, [this, d, ti]{
        d->textInputs.removeAll(ti);
        disableTI(ti);
        ti->disconnect();
        ti->deleteLater();
    }); // textInputs only save and traverse text inputs, do not handle connections
    // Whether this text input belongs to current seat or not, we should connect
    // its requestFocus signal for it might request focus from another seat to activate
    // itself here. For example, text input v1.
    connect(ti, &WQuickTextInput::requestFocus, this, [this, ti]{
        if (ti->seat() && seatName() == ti->seat()->name()) {
            connectToTI(ti);
            if (seat()->keyboardFocus()) {
                ti->sendEnter(seat()->keyboardFocus());
            }
        }
    });
    if (ti->seat() && seatName() == ti->seat()->name()) {
        connectToTI(ti);
    }
}

void WInputMethodHelper::handleTIEnabled()
{
    WQuickTextInput *ti = qobject_cast<WQuickTextInput*>(sender());
    Q_ASSERT(ti);
    W_D(WInputMethodHelper);
    auto im = inputMethod();
    auto activeTI = focusedTextInput();
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
    setFocusedTextInput(ti);
    // Try to activate input method.
    if (im) {
        im->sendActivate();
        im->sendDone();
    }
}

void WInputMethodHelper::handleTIDisabled()
{
    WQuickTextInput *ti = qobject_cast<WQuickTextInput*>(sender());
    disableTI(ti);
}

void WInputMethodHelper::handleFocusedTICommitted()
{
    auto ti = focusedTextInput();
    Q_ASSERT(ti);
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
    setCursorRect(ti->cursorRect());
    updateAllPopupSurfaces(ti->cursorRect());
}

void WInputMethodHelper::handleIMCommitted()
{
    auto im = inputMethod();
    Q_ASSERT(im);
    auto ti = focusedTextInput();
    if (ti) {
        ti->handleIMCommitted(im);
    }
}

void WInputMethodHelper::notifyLeave()
{
    if (auto ti = focusedTextInput()) {
        ti->sendLeave();
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
    Q_ASSERT(popup->handle());
    popup->handle()->send_text_input_rectangle(cursorRect);
}

void WInputMethodHelper::setSeat(WQuickSeat *seat)
{
    W_D(WInputMethodHelper);
    if (d->seat == seat)
        return;
    if (d->seat) {
        d->seat->disconnect(this);
    }
    d->seat = seat;
    if (seat) {
        connect(d->seat, &WQuickSeat::keyboardFocusChanged, this, &WInputMethodHelper::resendKeyboardFocus);
    }
    Q_EMIT seatChanged();
}

WSeat *WInputMethodHelper::wseat() const
{
    return seat()->seat();
}
WAYLIB_SERVER_END_NAMESPACE
