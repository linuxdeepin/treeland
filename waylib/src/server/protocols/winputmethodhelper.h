// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <qwglobal.h>

#include <QObject>

Q_MOC_INCLUDE("winputpopupsurface.h")

QW_BEGIN_NAMESPACE
class qw_input_method_v2;
class qw_input_method_keyboard_grab_v2;
class qw_input_popup_surface_v2;
class qw_virtual_keyboard_v1;
QW_END_NAMESPACE
struct wlr_seat_keyboard_grab;
struct wlr_virtual_keyboard_v1;
struct wlr_keyboard_modifiers;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSeat;
class WInputDevice;
class WInputMethodV2;
class WInputMethodHelperPrivate;
class WInputPopupSurface;
class WTextInput;
class WAYLIB_SERVER_EXPORT WInputMethodHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodHelper)

public:
    explicit WInputMethodHelper(WServer *server, WSeat *seat);
    ~WInputMethodHelper() override;

Q_SIGNALS:
    void inputPopupSurfaceV2Added(WInputPopupSurface *popupSurface);
    void inputPopupSurfaceV2Removed(WInputPopupSurface *popupSurface);

private:
    const QList<WInputDevice *> &virtualKeyboards() const;
    void handleNewTI(WTextInput *ti);
    void handleNewIMV2(QW_NAMESPACE::qw_input_method_v2 *imv2);
    void handleNewKGV2(QW_NAMESPACE::qw_input_method_keyboard_grab_v2 *kgv2);
    void handleNewIPSV2(QW_NAMESPACE::qw_input_popup_surface_v2 *ipsv2);
    void handleNewVKV1(wlr_virtual_keyboard_v1 *vkv1);
    void updateAllPopupSurfaces(QRect cursorRect);
    void updatePopupSurface(WInputPopupSurface *popup, QRect cursorRect);
    void notifyLeave();
    void resendKeyboardFocus();
    void connectToTI(WTextInput *ti);
    void disableTI(WTextInput *ti);
    void handleTIEnabled();
    void handleTIDisabled();
    void handleFocusedTICommitted();
    void handleIMCommitted();
    WTextInput *focusedTextInput() const;
    void setFocusedTextInput(WTextInput *ti);
    WInputMethodV2 *inputMethod() const;
    void setInputMethod(WInputMethodV2 *im);
    QW_NAMESPACE::qw_input_method_keyboard_grab_v2 *activeKeyboardGrab() const;
    friend void handleKey(struct wlr_seat_keyboard_grab *grab, uint32_t time_msec, uint32_t key, uint32_t state);
    friend void handleModifiers(struct wlr_seat_keyboard_grab *grab, const struct wlr_keyboard_modifiers *modifiers);
};

WAYLIB_SERVER_END_NAMESPACE
