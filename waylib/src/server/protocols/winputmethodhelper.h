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
class WInputPopupSurface;
class WTextInput;
class WSurface;
class WAYLIB_SERVER_EXPORT WInputMethodHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodHelper)

public:
    explicit WInputMethodHelper(WServer *server, WSeat *seat);
    ~WInputMethodHelper() override;

    WSurface *textInputFocusSurface() const;
    QRect textInputCursorRect() const;

Q_SIGNALS:
    void inputPopupSurfaceV2Added(WInputPopupSurface *popupSurface);
    void inputPopupSurfaceV2Removed(WInputPopupSurface *popupSurface);
    void textInputCursorRectChanged(QRect cursorRect);

private:
    void handleNewTI(WTextInput *ti);
    void handleNewIMV2(wlr_input_method_v2 *imv2);
    void handleNewKGV2(wlr_input_method_keyboard_grab_v2 *kgv2);
    void handleNewIPSV2(wlr_input_popup_surface_v2 *ipsv2);
    void handleNewVKV1(wlr_virtual_keyboard_v1 *vkv1);
    void updateAllPopupSurfaces(QRect cursorRect);
    void updatePopupSurface(WInputPopupSurface *popup, QRect cursorRect);
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
