// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

#include <text-input-unstable-v1-protocol.h>

QW_BEGIN_NAMESPACE
class QWDisplay;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
struct ws_text_input_v1;

class WSurface;
class WSeat;
class WQuickTextInputV1Private;
class WQuickTextInputManagerV1Private;

class WQuickTextInputV1 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputV1)
    QML_NAMED_ELEMENT(TextInputV1)
    QML_UNCREATABLE("Only created in C++ by WQuickTextInputManagerV1.")
    Q_PROPERTY(WSeat* seat READ seat NOTIFY seatChanged FINAL)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    Q_PROPERTY(QString surroundingText READ surroundingText NOTIFY surroundingTextChanged FINAL)
    Q_PROPERTY(quint32 surroundingTextCursor READ surroundingTextCursor NOTIFY surroundingTextChanged FINAL)
    Q_PROPERTY(quint32 surroundingTextAnchor READ surroundingTextAnchor NOTIFY surroundingTextChanged FINAL)
    Q_PROPERTY(ContentHint contentHint READ contentHint NOTIFY contentTypeChanged FINAL)
    Q_PROPERTY(ContentPurpose contentPurpose READ contentPurpose NOTIFY contentTypeChanged FINAL)
    Q_PROPERTY(QRect cursorRectangle READ cursorRectangle NOTIFY cursorRectangleChanged FINAL)
    Q_PROPERTY(QString preferredLanguage READ preferredLanguage NOTIFY preferredLanguageChanged FINAL)
    Q_PROPERTY(WSurface* focusedSurface READ focusedSurface NOTIFY focusedSurfaceChanged FINAL)

public:
    enum ContentHint {
        CH_None = ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE,
        CH_Default = ZWP_TEXT_INPUT_V1_CONTENT_HINT_DEFAULT,
        CH_Password = ZWP_TEXT_INPUT_V1_CONTENT_HINT_PASSWORD,
        CH_AutoCompletion = ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION,
        CH_AutoCorrection = ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION,
        CH_AutoCapitalization = ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION,
        CH_Lowercase = ZWP_TEXT_INPUT_V1_CONTENT_HINT_LOWERCASE,
        CH_Uppercase = ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE,
        CH_Titlecase = ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE,
        CH_HiddenText = ZWP_TEXT_INPUT_V1_CONTENT_HINT_HIDDEN_TEXT,
        CH_SensitiveData = ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA,
        CH_Latin = ZWP_TEXT_INPUT_V1_CONTENT_HINT_LATIN,
        CH_Multiline = ZWP_TEXT_INPUT_V1_CONTENT_HINT_MULTILINE
    };
    Q_ENUM(ContentHint)

    enum ContentPurpose {
        CP_Normal = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL,
        CP_Alpha = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_ALPHA,
        CP_Digits = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DIGITS,
        CP_Number = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER,
        CP_Phone = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE,
        CP_Url = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL,
        CP_Email = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL,
        CP_Name = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NAME,
        CP_Password = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD,
        CP_Date = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE,
        CP_Time = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME,
        CP_Datetime= ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME,
        CP_Terminal = ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TERMINAL
    };
    Q_ENUM(ContentPurpose)

    enum PreeditStyle {
        PS_Default = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT,
        PS_None = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE,
        PS_Active = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE,
        PS_Inactive = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE,
        PS_Highlight = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT,
        PS_Underline = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE,
        PS_Selection = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION,
        PS_Incorrect = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT
    };
    Q_ENUM(PreeditStyle)

    enum TextDirection {
        TD_Auto = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_AUTO,
        TD_Ltr = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_LTR,
        TD_Rtl = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_RTL
    };
    Q_ENUM(TextDirection)

    WSeat *seat() const;
    bool active() const;
    QString surroundingText() const;
    quint32 surroundingTextCursor() const;
    quint32 surroundingTextAnchor() const;
    ContentHint contentHint() const;
    ContentPurpose contentPurpose() const;
    QRect cursorRectangle() const;
    QString preferredLanguage() const;
    WSurface *focusedSurface() const;

Q_SIGNALS:
    void seatChanged();
    void activeChanged();
    void showInputPanel();
    void hideInputPanel();
    void reset();
    void surroundingTextChanged();
    void contentTypeChanged();
    void cursorRectangleChanged();
    void preferredLanguageChanged();
    void commit();
    void invoke(quint32 button, quint32 index);
    void beforeDestroy();
    void focusedSurfaceChanged();

public Q_SLOTS:
    void sendEnter(WSurface *surface);
    void sendLeave();
    void sendModifiersMap(QStringList modifiers);
    void sendInputPanelState(quint32 state);
    void sendPreeditString(QString text, QString commit);
    void sendPreeditStyling(quint32 index, quint32 length, PreeditStyle style);
    void sendPreeditCursor(qint32 index);
    void sendCommitString(QString text);
    void sendCursorPosition(qint32 index, qint32 anchor);
    void sendDeleteSurroundingText(qint32 index, quint32 length);
    void sendKeySym(quint32 time, quint32 sym, quint32 state, quint32 modifiers);
    void sendLanguage(QString language);
    void sendTextDirection(TextDirection textDirection);

private:
    explicit WQuickTextInputV1(ws_text_input_v1 *handle, QObject *parent = nullptr);
    friend class WQuickTextInputManagerV1Private;
};

class WQuickTextInputManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputManagerV1)
    QML_NAMED_ELEMENT(TextInputManagerV1)
    Q_PROPERTY(QList<WQuickTextInputV1*> textInputs READ textInputs FINAL)

public:
    explicit WQuickTextInputManagerV1(QObject *parent = nullptr);
    QList<WQuickTextInputV1 *> textInputs() const;

Q_SIGNALS:
    void beforeDestroy();
    void newTextInput(WQuickTextInputV1 *newTextInput);

protected:
    void create() override;
};
WAYLIB_SERVER_END_NAMESPACE
