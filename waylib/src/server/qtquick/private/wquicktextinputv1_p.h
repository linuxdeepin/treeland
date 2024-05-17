// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wquickwaylandserver.h"
#include "winputmethodcommon_p.h"

#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

QW_BEGIN_NAMESPACE
class QWDisplay;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
struct ws_text_input_v1;
struct ws_text_input_manager_v1;
class WSurface;
class WSeat;
class WTextInputV1;
class WTextInputManagerV1Private;
class WTextInputV1Private;
class WTextInputManagerV1 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputManagerV1)
public:
    static WTextInputManagerV1 *create(QW_NAMESPACE::QWDisplay *display);

Q_SIGNALS:
    void newTextInput(WTextInputV1 *ti);

private:
    explicit WTextInputManagerV1(ws_text_input_manager_v1 *handle);
};

class WTextInputV1 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputV1)
    Q_PROPERTY(WSeat *seat READ seat NOTIFY seatChanged FINAL)
    Q_PROPERTY(WSurface *focusedSurface READ focusedSurface NOTIFY focusedSurfaceChanged FINAL)
    Q_PROPERTY(QString surroundingText READ surroundingText NOTIFY surroundingTextChanged FINAL)
    Q_PROPERTY(uint surroundingCursor READ surroundingCursor NOTIFY surroundingTextChanged FINAL)
    Q_PROPERTY(uint surroundingAnchor READ surroundingAnchor NOTIFY surroundingTextChanged FINAL)
    Q_PROPERTY(uint contentHint READ contentHint NOTIFY contentTypeChanged FINAL)
    Q_PROPERTY(uint contentPurpose READ contentPurpose NOTIFY contentTypeChanged FINAL)
    Q_PROPERTY(QRect cursorRectangle READ cursorRectangle NOTIFY cursorRectangleChanged FINAL)
    Q_PROPERTY(QString preferredLanguage READ preferredLanguage NOTIFY preferredLanguageChanged FINAL)

public:
    WSeat *seat() const;
    WSurface *focusedSurface() const;
    QString surroundingText() const;
    uint surroundingCursor() const;
    uint surroundingAnchor() const;
    uint contentHint() const;
    uint contentPurpose() const;
    QRect cursorRectangle() const;
    QString preferredLanguage() const;

Q_SIGNALS:
    void activate(WSeat *seat, WSurface *surface);
    void deactivate(WSeat *seat);
    void showInputPanel();
    void hideInputPanel();
    void reset();
    void commitState();
    void invokeAction(uint button, uint index);
    void beforeDestroy();
    // As text input v1 does not have a double-buffer mechanism, we should provide change signal for property
    // This is necessary for QML to perform property binding
    void surroundingTextChanged(); // text, cursor and anchor
    void contentTypeChanged(); // hint and purpose
    void cursorRectangleChanged();
    void preferredLanguageChanged();
    void seatChanged(WSeat *seat);
    void focusedSurfaceChanged(WSurface *surface);

public Q_SLOTS:
    void sendEnter(WSurface *surface);
    void sendLeave();
    void sendModifiersMap(QStringList map);
    void sendInputPanelState(uint state);
    void sendPreeditString(const QString &text, const QString &commit);
    void sendPreeditStyling(uint index, uint length, uint style);
    void sendPreeditCursor(int index);
    void sendCommitString(const QString &text);
    void sendCursorPosition(int index, int anchor);
    void sendDeleteSurroundingString(int index, uint length);
    void sendKeysym(uint time, uint sym, uint state, uint modifiers);
    void sendLanguage(const QString &language);
    void sendTextDirection(uint direction);

private:
    explicit WTextInputV1(ws_text_input_v1 *handle, QObject *parent = nullptr);
    friend class WTextInputManagerV1Private;
};
class WSurface;
class WSeat;
class WTextInputV1;
class WTextInputManagerV1;
class WQuickTextInputV1Private;
class WQuickTextInputManagerV1Private;
class WQuickTextInputV1 : public WQuickTextInput
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputV1)

public:
    enum ContentHint {
        CH_None = 0x0,
        CH_Default = 0x7,
        CH_Password = 0xc0,
        CH_AutoCompletion = 0x1,
        CH_AutoCorrection = 0x2,
        CH_AutoCapitalization = 0x4,
        CH_Lowercase = 0x8,
        CH_Uppercase = 0x10,
        CH_Titlecase = 0x20,
        CH_HiddenText = 0x40,
        CH_SensitiveData = 0x80,
        CH_Latin = 0x100,
        CH_Multiline = 0x200
    };
    Q_FLAG(ContentHint)
    Q_DECLARE_FLAGS(ContentHints, ContentHint)

    enum ContentPurpose {
        CP_Normal,
        CP_Alpha,
        CP_Digits,
        CP_Number,
        CP_Phone,
        CP_Url,
        CP_Email,
        CP_Name,
        CP_Password,
        CP_Date,
        CP_Time,
        CP_Datetime,
        CP_Terminal
    };
    Q_ENUM(ContentPurpose)

    enum PreeditStyle {
        PS_Default,
        PS_None,
        PS_Active,
        PS_Inactive,
        PS_Highlight,
        PS_Underline,
        PS_Selection,
        PS_Incorrect
    };
    Q_ENUM(PreeditStyle)

    enum TextDirection {
        TD_Auto,
        TD_Ltr,
        TD_Rtl
    };
    Q_ENUM(TextDirection)

    explicit WQuickTextInputV1(WTextInputV1 *handle, QObject *parent = nullptr);
    WSeat *seat() const override;
    QString surroundingText() const override;
    int surroundingCursor() const override;
    int surroundingAnchor() const override;
    IME::ContentHints contentHints() const override;
    IME::ContentPurpose contentPurpose() const override;
    QRect cursorRect() const override;
    WSurface *focusedSurface() const override;
    IME::Features features() const override;


public Q_SLOTS:
    void sendEnter(WSurface *surface) override;
    void sendLeave() override;
    void sendDone() override;
    void handleIMCommitted(WQuickInputMethodV2 *im) override;

private:
    void sendModifiersMap(QStringList modifiers);
    void sendInputPanelState(uint state);
    void sendPreeditString(QString text, QString commit);
    void sendPreeditStyling(uint index, uint length, PreeditStyle style);
    void sendPreeditCursor(int index);
    void sendCommitString(QString text);
    void sendCursorPosition(int index, int anchor);
    void sendDeleteSurroundingText(int index, uint length);
    void sendKeySym(uint time, uint sym, uint state, uint modifiers);
    void sendLanguage(QString language);
    void sendTextDirection(TextDirection textDirection);
    WTextInputV1 *handle() const;
};

class WQuickTextInputManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputManagerV1)
    QML_NAMED_ELEMENT(TextInputManagerV1)

public:
    explicit WQuickTextInputManagerV1(QObject *parent = nullptr);

Q_SIGNALS:
    void newTextInput(WTextInputV1 *textInput);

protected:
    void create() override;
};
WAYLIB_SERVER_END_NAMESPACE
