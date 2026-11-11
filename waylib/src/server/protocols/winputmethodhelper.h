// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "wglobal.h"

#include <QObject>

Q_MOC_INCLUDE("winputpopupsurface.h")

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSeat;
class WInputDevice;
class WInputMethodV2;
class WInputMethodHelperPrivate;
class WInputMethodManagerV2;
class WTextInputManagerV1;
class WTextInputManagerV2;
class WTextInputManagerV3;
class WVirtualKeyboardManagerV1;
class WInputPopupSurface;
class WTextInput;
class WSurface;
class WAYLIB_SERVER_EXPORT WInputMethodHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodHelper)

public:
    struct Protocols
    {
        WInputMethodManagerV2 *inputMethod = nullptr;
        WTextInputManagerV1 *textInputV1 = nullptr;
        WTextInputManagerV2 *textInputV2 = nullptr;
        WTextInputManagerV3 *textInputV3 = nullptr;
        WVirtualKeyboardManagerV1 *virtualKeyboard = nullptr;
    };

    explicit WInputMethodHelper(WServer *server, WSeat *seat, const Protocols &protocols);
    ~WInputMethodHelper() override;

    WSurface *textInputFocusSurface() const;
    QRect textInputCursorRect() const;
    bool inputMethodConnected() const;

public Q_SLOTS:
    // End the active input-method session without destroying the helper.
    // Session switching calls this before the old user session's seat is
    // detached, so a held composition cannot survive into another user's
    // desktop.
    void deactivate();

Q_SIGNALS:
    void inputPopupSurfaceV2Added(WInputPopupSurface *popupSurface);
    void inputPopupSurfaceV2Removed(WInputPopupSurface *popupSurface);
    void textInputCursorRectChanged(QRect cursorRect);
    // Emitted after the text-input focus settled when the surface returned by
    // textInputFocusSurface() changed (window switch, new anchor, commit routed
    // elsewhere). Consumers may re-anchor input popup surfaces on it.
    void textInputFocusSurfaceChanged(WSurface *surface);
    // Emitted when the seat's input method is accepted (true) or destroyed
    // (false). The session daemon uses the false edge to hand a fresh Wayland
    // socket to a surviving input-method process after a replacement race.
    void inputMethodChanged(bool connected);

private:
    void handleNewTI(WTextInput *ti);
    void handleNewIMV2(wlr_input_method_v2 *imv2);
    void handleNewKGV2(wlr_input_method_keyboard_grab_v2 *kgv2);
    void handleNewIPSV2(wlr_input_popup_surface_v2 *ipsv2);
    void handleNewVKV1(wlr_virtual_keyboard_v1 *vkv1);
    void updateAllPopupSurfaces(QRect cursorRect);
    void updatePopupSurface(WInputPopupSurface *popup, QRect cursorRect);
    void notifyLeave();
    void resendKeyboardFocus();
    void reconcileTextInput(const char *reason);
    void connectToTI(WTextInput *ti);
    void disableTI(WTextInput *ti);
    void handleTIEnabled();
    void handleTIDisabled();
    void handleFocusedTICommitted();
    void handleIMCommitted();
    void handleActiveIMDestroyed();
    WTextInput *enabledTextInput() const;
    void setEnabledTextInput(WTextInput *ti);
    WInputMethodV2 *inputMethod() const;
    void setInputMethod(WInputMethodV2 *im);
};

WAYLIB_SERVER_END_NAMESPACE
