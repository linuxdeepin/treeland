// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>

Q_MOC_INCLUDE("wquickseat_p.h")
Q_MOC_INCLUDE("wquicktextinputv1_p.h")
Q_MOC_INCLUDE("wquicktextinputv3_p.h")
Q_MOC_INCLUDE("wquickinputmethodv2_p.h")
Q_MOC_INCLUDE("wquickvirtualkeyboardv1_p.h")
QW_USE_NAMESPACE

QW_BEGIN_NAMESPACE
class QWInputMethodV2;
class QWInputMethodKeyboardGrabV2;
class QWInputPopupSurfaceV2;
class QWVirtualKeyboardV1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickSeat;
class WQuickTextInputManagerV3;
class WQuickInputMethodManagerV2;
class WQuickInputMethodKeyboardGrabV2;
class WQuickInputMethodV2;
class WQuickVirtualKeyboardManagerV1;
class WQuickVirtualKeyboardV1;
class WInputMethodHelperPrivate;
class WSeat;
class WInputPopupV2;
class WQuickTextInput;
class WQuickInputPopupSurfaceV2;
class WQuickTextInputManagerV1;
class WInputMethodHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodHelper)
    QML_NAMED_ELEMENT(InputMethodHelper)
    Q_PROPERTY(WQuickSeat *seat READ seat WRITE setSeat NOTIFY seatChanged FINAL REQUIRED)
    Q_PROPERTY(WQuickTextInputManagerV1 *textInputManagerV1 READ textInputManagerV1 WRITE setTextInputManagerV1 NOTIFY textInputManagerV1Changed FINAL)
    Q_PROPERTY(WQuickTextInputManagerV3 *textInputManagerV3 READ textInputManagerV3 WRITE setTextInputManagerV3 NOTIFY textInputManagerV3Changed FINAL)
    Q_PROPERTY(WQuickInputMethodManagerV2 *inputMethodManagerV2 READ inputMethodManagerV2 WRITE setInputMethodManagerV2 NOTIFY inputMethodManagerV2Changed FINAL REQUIRED)
    Q_PROPERTY(WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1 READ virtualKeyboardManagerV1 WRITE setVirtualKeyboardManagerV1 NOTIFY virtualKeyboardManagerV1Changed FINAL REQUIRED)
    Q_PROPERTY(QQuickItem* activeFocusItem READ activeFocusItem WRITE setActiveFocusItem NOTIFY activeFocusItemChanged FINAL REQUIRED)
    Q_PROPERTY(QRect cursorRect READ cursorRect NOTIFY cursorRectChanged FINAL)



public:
    explicit WInputMethodHelper(QObject *parent = nullptr);

    WQuickSeat *seat() const;
    void setSeat(WQuickSeat *qseat);
    WSeat *wseat() const;
    QString seatName() const;
    WQuickTextInputManagerV3 *textInputManagerV3() const;
    void setTextInputManagerV3(WQuickTextInputManagerV3 *textInputManagerV3);
    WQuickTextInputManagerV1 *textInputManagerV1() const;
    void setTextInputManagerV1(WQuickTextInputManagerV1 *textInputManagerV1);
    WQuickInputMethodManagerV2 *inputMethodManagerV2() const;
    void setInputMethodManagerV2(WQuickInputMethodManagerV2 *inputMethodManagerV2);
    WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1() const;
    void setVirtualKeyboardManagerV1(WQuickVirtualKeyboardManagerV1 *virtualKeyboardManagerV1);
    WQuickTextInput *focusedTextInput() const;
    void setFocusedTextInput(WQuickTextInput *ti);
    WQuickInputMethodV2 *inputMethod() const;
    void setInputMethod(WQuickInputMethodV2 *im);
    QRect cursorRect() const;
    void setCursorRect(const QRect &rect);
    QQuickItem *activeFocusItem() const;
    void setActiveFocusItem(QQuickItem *item);
    WQuickInputMethodKeyboardGrabV2 *activeKeyboardGrab() const;
    QList<WQuickVirtualKeyboardV1 *> virtualKeyboards() const;

Q_SIGNALS:
    void inputPopupSurfaceV2Added(WInputPopupV2 *popupSurface);
    void inputPopupSurfaceV2Removed(WInputPopupV2 *popupSurface);
    void cursorRectChanged();
    void activeFocusItemChanged();
    void seatChanged();
    void textInputManagerV1Changed();
    void textInputManagerV3Changed();
    void inputMethodManagerV2Changed();
    void virtualKeyboardManagerV1Changed();
    void activeTextInputChanged();
    void activeInputMethodChanged();

private:
    void handleNewTI(QObject *ti);
    void handleNewIMV2(QWInputMethodV2 *imv2);
    void handleNewKGV2(QWInputMethodKeyboardGrabV2 *kgv2);
    void handleNewIPSV2(QWInputPopupSurfaceV2 *ipsv2);
    void handleNewVKV1(QWVirtualKeyboardV1 *vkv1);
    void updateAllPopupSurfaces(QRect cursorRect);
    void updatePopupSurface(WInputPopupV2 *popup, QRect cursorRect);
    void notifyLeave();
    void resendKeyboardFocus();
    void connectToTI(WQuickTextInput *ti);
    void disableTI(WQuickTextInput *ti);
    void tryAddTextInput(WQuickTextInput *ti);
    void handleTIEnabled();
    void handleTIDisabled();
    void handleFocusedTICommitted();
    void handleIMCommitted();
};

WAYLIB_SERVER_END_NAMESPACE
