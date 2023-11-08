// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wquickseat_p.h")
Q_MOC_INCLUDE("wquicktextinputv3_p.h")
Q_MOC_INCLUDE("wquickinputmethodv2_p.h")
Q_MOC_INCLUDE("wquickvirtualkeyboardv1_p.h")

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickSeat;
class WQuickTextInputManagerV3;
class WQuickTextInputV3;
class WQuickInputMethodManagerV2;
class WQuickInputMethodKeyboardGrabV2;
class WQuickInputMethodV2;
class WQuickVirtualKeyboardManagerV1;
class WQuickVirtualKeyboardV1;
class WInputMethodHelperPrivate;
class WSurface;
class WSeat;
class WInputPopupV2;
class WQuickInputPopupSurfaceV2;
class WInputMethodHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodHelper)
    QML_NAMED_ELEMENT(InputMethodHelper)
    Q_PROPERTY(WQuickSeat *seat READ seat WRITE setSeat FINAL REQUIRED)
    Q_PROPERTY(WQuickTextInputManagerV3 *textInputManagerV3 READ textInputManagerV3 WRITE setTextInputManagerV3 FINAL REQUIRED)
    Q_PROPERTY(WQuickInputMethodManagerV2 *inputMethodManagerV2 READ inputMethodManagerV2 WRITE setInputMethodManagerV2 FINAL REQUIRED)
    Q_PROPERTY(WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1 READ virtualKeyboardManagerV1 WRITE setVirtualKeyboardManagerV1 FINAL REQUIRED)
    Q_PROPERTY(WQuickTextInputV3 *activeTextInputV3 READ activeTextInputV3 WRITE setActiveTextInputV3 NOTIFY activeTextInputV3Changed FINAL)
    Q_PROPERTY(WQuickInputMethodV2 *inputMethodV2 READ inputMethodV2 WRITE setInputMethodV2 NOTIFY inputMethodV2Changed FINAL)

public:
    explicit WInputMethodHelper(QObject *parent = nullptr);

    WQuickSeat *seat() const;
    void setSeat(WQuickSeat *newSeat);
    WSeat *wseat() const;
    WQuickTextInputManagerV3 *textInputManagerV3() const;
    void setTextInputManagerV3(WQuickTextInputManagerV3 *textInputManagerV3);
    WQuickInputMethodManagerV2 *inputMethodManagerV2() const;
    void setInputMethodManagerV2(WQuickInputMethodManagerV2 *inputMethodManagerV2);
    WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1() const;
    void setVirtualKeyboardManagerV1(WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1);
    WQuickTextInputV3 *activeTextInputV3() const;
    void setActiveTextInputV3(WQuickTextInputV3 *newActiveTextInputV3);
    WQuickInputMethodV2 *inputMethodV2() const;
    void setInputMethodV2(WQuickInputMethodV2 *newInputMethodV2);

Q_SIGNALS:
    void activeTextInputV3Changed(WQuickTextInputV3 *newActiveTextInputV3);
    void inputMethodV2Changed(WQuickInputMethodV2 *newInputMethod);
    void inputPopupSurfaceV2Added(WInputPopupV2 *surfaceItem);
    void inputPopupSurfaceV2Removed(WInputPopupV2 *surfaceItem);

private:
    void onNewInputMethodV2(WQuickInputMethodV2 *newInputMethod);
    void onNewTextInputV3(WQuickTextInputV3 *newTextInput);
    void onNewVirtualKeyboardV1(WQuickVirtualKeyboardV1 *newVirtualKeyboard);
    void onTextInputV3Enabled(WQuickTextInputV3 *enabledTextInput);
    void onTextInputV3Disabled(WQuickTextInputV3 *disabledTextInput);
    void onTextInputV3Committed(WQuickTextInputV3 *textInput);
    void onTextInputV3Destroyed(WQuickTextInputV3 *destroyedTextInput);
    void onInputMethodV2Committed();
    void onKeyboardFocusChanged();
    void onNewKeyboardGrab(WQuickInputMethodKeyboardGrabV2 *keyboardGrab);
    void onInputPopupSurface(WQuickInputPopupSurfaceV2 *popupSurface);
    void onKeyboardFocusLost();

    void sendInputMethodV2State(WQuickTextInputV3 *textInputV3);
    void updateAllPopupSurfaces();
    void updatePopupSurface(WInputPopupV2 *popup);
};

WAYLIB_SERVER_END_NAMESPACE
