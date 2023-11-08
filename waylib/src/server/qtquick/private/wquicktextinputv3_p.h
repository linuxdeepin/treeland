// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <qwglobal.h>

#include <QQmlEngine>
#include <QRect>

Q_MOC_INCLUDE("wsurfaceitem.h")
Q_MOC_INCLUDE("wtoplevelsurface.h")
Q_MOC_INCLUDE("wsurface.h")
Q_MOC_INCLUDE("wseat.h")
Q_MOC_INCLUDE(<qwtextinputv3.h>)

QW_BEGIN_NAMESPACE
class QWTextInputV3;
class QWTextInputManagerV3;
QW_END_NAMESPACE

struct wlr_text_input_v3_state;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickTextInputV3Private;
class WToplevelSurface;
class WSurfaceItem;
class WSurface;
class WQuickTextInputV3;
class WQuickTextInputManagerV3Private;
class WSeat;
class WTextInputV3State;

class WAYLIB_SERVER_EXPORT WQuickTextInputManagerV3 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickTextInputManagerV3)
    QML_NAMED_ELEMENT(TextInputManagerV3)
    Q_PROPERTY(QList<WQuickTextInputV3 *> textInputs READ textInputs)

public:
    explicit WQuickTextInputManagerV3(QObject *parent = nullptr);
    QList<WQuickTextInputV3 *> textInputs() const;

Q_SIGNALS:
    void newTextInput(WQuickTextInputV3 *ti);

private:
    void create() override;
};

class WQuickTextInputV3 : public QObject, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TextInputV3)
    QML_UNCREATABLE("Only created by TextInputManagerV3 in C++")
    W_DECLARE_PRIVATE(WQuickTextInputV3)

    Q_PROPERTY(WSeat *seat READ seat CONSTANT FINAL)
    Q_PROPERTY(WTextInputV3State *state READ state CONSTANT FINAL)
    Q_PROPERTY(WTextInputV3State *pendingState READ pendingState CONSTANT FINAL)
    Q_PROPERTY(WSurface *focusedSurface READ focusedSurface FINAL)

public:
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
    Q_ENUM(ContentHint)

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

    WSeat *seat() const;
    WTextInputV3State *state() const;
    WTextInputV3State *pendingState() const;
    WSurface *focusedSurface() const;
    wl_client *waylandClient() const;
    QW_NAMESPACE::QWTextInputV3 *handle() const;

public Q_SLOTS:
    void sendEnter(WSurface *surface);
    void sendLeave();
    void sendPreeditString(const QString &text, qint32 cursorBegin, qint32 cursorEnd);
    void sendCommitString(const QString &text);
    void sendDeleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    void sendDone();

Q_SIGNALS:
    void enable();
    void disable();
    void commit();

private:
    WQuickTextInputV3(QW_NAMESPACE::QWTextInputV3 *handle, QObject *parent);
    friend class WQuickTextInputManagerV3;
};

class WTextInputV3StatePrivate;
class WTextInputV3State : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputV3State)
    Q_PROPERTY(QString surroundingText READ surroundingText FINAL)
    Q_PROPERTY(quint32 surroundingCursor READ surroundingCursor FINAL)
    Q_PROPERTY(quint32 surroundingAnchor READ surroundingAnchor FINAL)
    Q_PROPERTY(WQuickTextInputV3::ChangeCause textChangeCause READ textChangeCause FINAL)
    Q_PROPERTY(WQuickTextInputV3::ContentHint contentHint READ contentHint FINAL)
    Q_PROPERTY(WQuickTextInputV3::ContentPurpose contentPurpose READ contentPurpose FINAL)
    Q_PROPERTY(QRect cursorRect READ cursorRect FINAL)
    Q_PROPERTY(Features features READ features FINAL)

public:
    enum Feature {
        SurroundingText = 1 << 0,
        ContentType = 1 << 1,
        CursorRect = 1 << 2
    };
    Q_FLAG(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    wlr_text_input_v3_state *handle() const;
    QString surroundingText() const;
    quint32 surroundingCursor() const;
    quint32 surroundingAnchor() const;
    WQuickTextInputV3::ChangeCause textChangeCause() const;
    WQuickTextInputV3::ContentHint contentHint() const;
    WQuickTextInputV3::ContentPurpose contentPurpose() const;
    QRect cursorRect() const;
    Features features() const;

    friend QDebug operator<<(QDebug debug, const WTextInputV3State &state);

private:
    explicit WTextInputV3State(wlr_text_input_v3_state *handle, QObject *parent = nullptr);
    friend class WQuickTextInputV3Private;
};
WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WTextInputV3State::Features)
