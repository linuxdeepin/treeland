// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wserver.h"
#include "wtextinput_p.h"

#include <qwglobal.h>

#include <QQmlEngine>
#include <QRect>

Q_MOC_INCLUDE("wsurface.h")
Q_MOC_INCLUDE("wseat.h")
Q_MOC_INCLUDE(<qwtextinputv3.h>)

QW_BEGIN_NAMESPACE
class qw_text_input_v3;
class qw_text_input_manager_v3;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WSeat;
class WTextInputV3Private;
class WTextInputManagerV3Private;
/**
 * @brief The text input unstable v3 protocol entity used in QML
 * text input v3 use a double buffer state, thus, surrounding text
 * , text change cause, content type and cursor rect should be
 * considered as changed when committed.
 */
class WAYLIB_SERVER_EXPORT WTextInputV3 : public WTextInput
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputV3)
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
    WTextInputV3(QW_NAMESPACE::qw_text_input_v3 *handle, QObject *parent);

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
    QW_NAMESPACE::qw_text_input_v3 *handle() const;


public Q_SLOTS:
    void sendEnter(WSurface *surface) override;
    void sendLeave() override;
    void sendDone() override;
    void handleIMCommitted(WInputMethodV2 *im) override;

private:
    void sendPreeditString(const QString &text, qint32 cursorBegin, qint32 cursorEnd);
    void sendCommitString(const QString &text);
    void sendDeleteSurroundingText(quint32 beforeLength, quint32 afterLength);
};

class WAYLIB_SERVER_EXPORT WTextInputManagerV3 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputManagerV3)
public:
    explicit WTextInputManagerV3(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void newTextInput(WTextInputV3 *textInput);

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(WTextInputV3::ContentHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(WTextInputV3::Features)
WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3::ChangeCause)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3::ContentHint)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3::ContentHints)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3::ContentPurpose)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3::Feature)
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3::Features)
