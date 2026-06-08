// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "private/wglobal_p.h"

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wsurface.h")
Q_MOC_INCLUDE("winputmethodv2_p.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

#define CHECK_ENUM(a, b) static_assert(static_cast<int>(a) == static_cast<int>(b), #a"is not equal to"#b)

namespace IME {
Q_NAMESPACE
enum ChangeCause {
    CC_InputMethod,
    CC_Other
};
Q_ENUM_NS(ChangeCause)

enum ContentHint {
    CH_None = 0x0,
    CH_Completion = 0x1,
    CH_Spellcheck = 0x2,
    CH_AutoCapitalization = 0x4,
    CH_Lowercase = 0x8,
    CH_Uppercase = 0x10,
    CH_Titlecase = 0x20,
    CH_HiddenText = 0x40,
    CH_SensitiveData = 0x80,
    CH_Latin = 0x100,
    CH_Multiline = 0x200
};
Q_FLAG_NS(ContentHint)
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
    CP_Pin,
    CP_Date,
    CP_Time,
    CP_Datetime,
    CP_Terminal
};
Q_ENUM_NS(ContentPurpose)

enum Feature {
    F_SurroundingText,
    F_ContentType,
    F_CursorRect
};
Q_FLAG_NS(Feature)
Q_DECLARE_FLAGS(Features, Feature)

enum KeyboardModifier {
    KM_Shift = 1 << 0,
    KM_Caps = 1 << 1,
    KM_Ctrl = 1 << 2,
    KM_Alt = 1 << 3,
    KM_Mod2 = 1 << 4,
    KM_Mod3 = 1 << 5,
    KM_Logo = 1 << 6,
    KM_Mod5 = 1 << 7,
};
Q_ENUM_NS(KeyboardModifier)
}

class WSeat;
class WSurface;
class WInputMethodV2;
class WTextInputPrivate;
class WAYLIB_SERVER_EXPORT WTextInput : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInput)
public:
    virtual WSeat *seat() const = 0;
    virtual WSurface *focusedSurface() const = 0;
    virtual QString surroundingText() const = 0;
    virtual int surroundingCursor() const = 0;
    virtual int surroundingAnchor() const = 0;
    virtual IME::ChangeCause textChangeCause() const { return IME::CC_InputMethod; }
    virtual IME::ContentHints contentHints() const { return IME::CH_None; }
    virtual IME::ContentPurpose contentPurpose() const { return IME::CP_Normal; }
    virtual QRect cursorRect() const = 0;
    virtual IME::Features features() const = 0;
    inline explicit WTextInput(QObject *parent = nullptr);
    inline explicit WTextInput(WTextInputPrivate& d, QObject *parent = nullptr);

Q_SIGNALS:
    void entityAboutToDestroy();
    void enabled();
    void disabled();
    void committed();
    void requestFocus(); // This is for text input v1's activate signal
    void requestLeave(); // For text input v1's deactivate signal

public Q_SLOTS:
    virtual void sendEnter(WSurface *surface) = 0;
    virtual void sendLeave() = 0;
    virtual void sendDone() = 0;
    virtual void handleIMCommitted(WInputMethodV2 *im) = 0;
};

class Q_DECL_HIDDEN WTextInputPrivate : public WObjectPrivate {
public:
    W_DECLARE_PUBLIC(WTextInput)
    explicit WTextInputPrivate(WTextInput *qq)
        : WObjectPrivate(qq)
    {}
};

WTextInput::WTextInput(QObject *parent)
    : WTextInput(*new WTextInputPrivate(this), parent)
{ }

inline WTextInput::WTextInput(WTextInputPrivate &d, QObject *parent)
    : QObject(parent)
    , WObject(d)
{ }
WAYLIB_SERVER_END_NAMESPACE
