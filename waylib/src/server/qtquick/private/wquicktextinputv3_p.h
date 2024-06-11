// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wquickwaylandserver.h"
#include "winputmethodcommon_p.h"

#include <qwglobal.h>

#include <QQmlEngine>
#include <QRect>

Q_MOC_INCLUDE("wsurface.h")
Q_MOC_INCLUDE("wseat.h")
Q_MOC_INCLUDE(<qwtextinputv3.h>)

QW_BEGIN_NAMESPACE
class QWTextInputV3;
class QWTextInputManagerV3;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WSeat;
class WQuickTextInputV3Private;
class WQuickTextInputManagerV3Private;
class WAYLIB_SERVER_EXPORT WQuickTextInputManagerV3 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputManagerV3)
    QML_NAMED_ELEMENT(TextInputManagerV3)

public:
    explicit WQuickTextInputManagerV3(QObject *parent = nullptr);

Q_SIGNALS:
    void newTextInput(QW_NAMESPACE::QWTextInputV3 *textInput);

private:
    WServerInterface *create() override;
};

/**
 * @brief The text input unstable v3 protocol entity used in QML
 * text input v3 use a double buffer state, thus, surrounding text
 * , text change cause, content type and cursor rect should be
 * considered as changed when committed.
 */
class WQuickTextInputV3 : public WQuickTextInput
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputV3)
public:
    enum Feature {
        SurroundingText = 1 << 0,
        ContentType = 1 << 1,
        CursorRect = 1 << 2
    };
    Q_FLAG(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    enum ChangeCause {
        InputMethod,
        Other
    };
    Q_ENUM(ChangeCause)

    enum ContentHint {
        None = 0x0,
        Completion = 0x1,
        Spellcheck = 0x2,
        AutoCapitalization = 0x4,
        Lowercase = 0x8,
        Uppercase = 0x10,
        Titlecase = 0x20,
        HiddenText = 0x40,
        SensitiveData = 0x80,
        Latin = 0x100,
        Multiline = 0x200
    };
    Q_FLAG(ContentHint)
    Q_DECLARE_FLAGS(ContentHints, ContentHint)

    enum ContentPurpose {
        Normal,
        Alpha,
        Digits,
        Number,
        Phone,
        Url,
        Email,
        Name,
        Password,
        Pin,
        Date,
        Time,
        Datetime,
        Terminal
    };
    Q_ENUM(ContentPurpose)
    WQuickTextInputV3(QW_NAMESPACE::QWTextInputV3 *handle, QObject *parent);

    WSeat *seat() const override;
    WSurface *focusedSurface() const override;
    QString surroundingText() const override;
    int surroundingCursor() const override;
    int surroundingAnchor() const override;
    IME::ChangeCause textChangeCause() const override;
    IME::ContentHints contentHints() const override;
    IME::ContentPurpose contentPurpose() const override;
    QRect cursorRect() const override;
    IME::Features features() const override;
    QW_NAMESPACE::QWTextInputV3 *handle() const;


public Q_SLOTS:
    void sendEnter(WSurface *surface) override;
    void sendLeave() override;
    void sendDone() override;
    void handleIMCommitted(WQuickInputMethodV2 *im) override;

private:
    void sendPreeditString(const QString &text, qint32 cursorBegin, qint32 cursorEnd);
    void sendCommitString(const QString &text);
    void sendDeleteSurroundingText(quint32 beforeLength, quint32 afterLength);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WQuickTextInputV3::ContentHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(WQuickTextInputV3::Features)
WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickTextInputV3::ChangeCause)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickTextInputV3::ContentHint)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickTextInputV3::ContentHints)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickTextInputV3::ContentPurpose)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickTextInputV3::Feature)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickTextInputV3::Features)
