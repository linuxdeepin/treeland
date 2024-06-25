// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodv2_p.h"
#include "wseat.h"
#include "winputdevice.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"

#include <qwcompositor.h>
#include <qwinputmethodv2.h>
#include <qwseat.h>
#include <qwkeyboard.h>
#include <qwvirtualkeyboardv1.h>

#include <QKeySequence>
#include <QLoggingCategory>
#include <QRect>

extern "C" {
#define delete delete_c
#include <wlr/types/wlr_input_method_v2.h>
#undef delete
#include <wlr/types/wlr_keyboard.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
#include <wlr/types/wlr_virtual_keyboard_v1.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class WInputMethodManagerV2Private : public WObjectPrivate
{
public:
    explicit WInputMethodManagerV2Private(WInputMethodManagerV2 *qq)
        : WObjectPrivate(qq)
    {}
    W_DECLARE_PUBLIC(WInputMethodManagerV2)

    QList<WInputMethodV2 *> inputMethods;
};

WInputMethodManagerV2::WInputMethodManagerV2(QObject *parent)
    : WObject(*new WInputMethodManagerV2Private(this), nullptr)
{ }

void WInputMethodManagerV2::create(WServer *server)
{
    auto handle = QWInputMethodManagerV2::create(server->handle());
    Q_ASSERT(handle);
    m_handle = handle;
    connect(handle, &QWInputMethodManagerV2::inputMethod, this, &WInputMethodManagerV2::newInputMethod);
}

class WInputMethodV2Private : public WWrapObjectPrivate
{
public:
    WInputMethodV2Private(QWInputMethodV2 *h, WInputMethodV2 *qq)
        : WWrapObjectPrivate(qq)
    {
        initHandle(h);
    }

    WWRAP_HANDLE_FUNCTIONS(QWInputMethodV2, wlr_input_method_v2)

    W_DECLARE_PUBLIC(WInputMethodV2)
};

WInputMethodV2::WInputMethodV2(QWInputMethodV2 *h, QObject *parent) :
    WWrapObject(*new WInputMethodV2Private(h, this), parent)
{
    W_D(WInputMethodV2);
    connect(handle(), &QWInputMethodV2::commit, this, &WInputMethodV2::committed);
    connect(handle(), &QWInputMethodV2::grabKeybord, this, &WInputMethodV2::newKeyboardGrab);
    connect(handle(), &QWInputMethodV2::newPopupSurface, this, &WInputMethodV2::newPopupSurface);
}

QWInputMethodV2 *WInputMethodV2::handle() const
{
    return d_func()->handle();
}

WSeat *WInputMethodV2::seat() const
{
    W_DC(WInputMethodV2);
    return WSeat::fromHandle(QWSeat::from(d->nativeHandle()->seat));
}

void WInputMethodV2::sendContentType(quint32 hint, quint32 purpose)
{
    W_D(WInputMethodV2);
    d->handle()->sendContentType(hint, purpose);
}

void WInputMethodV2::sendActivate()
{
    W_D(WInputMethodV2);
    d->handle()->sendActivate();
}

void WInputMethodV2::sendDeactivate()
{
    W_D(WInputMethodV2);
    d->handle()->sendDeactivate();
}

void WInputMethodV2::sendDone()
{
    W_D(WInputMethodV2);
    d->handle()->sendDone();
}

void WInputMethodV2::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    W_D(WInputMethodV2);
    d->handle()->sendSurroundingText(qPrintable(text), cursor, anchor);
}

void WInputMethodV2::sendTextChangeCause(quint32 cause)
{
    W_D(WInputMethodV2);
    d->handle()->sendTextChangeCause(cause);
}

void WInputMethodV2::sendUnavailable()
{
    W_D(WInputMethodV2);
    d->handle()->sendUnavailable();
}

QString WInputMethodV2::commitString() const
{
    return d_func()->nativeHandle()->current.commit_text;
}

uint WInputMethodV2::deleteSurroundingBeforeLength() const
{
    return d_func()->nativeHandle()->current.delete_c.before_length;
}

uint WInputMethodV2::deleteSurroundingAfterLength() const
{
    return d_func()->nativeHandle()->current.delete_c.after_length;
}

QString WInputMethodV2::preeditString() const
{
    return d_func()->nativeHandle()->current.preedit.text;
}

int WInputMethodV2::preeditCursorBegin() const
{
    return d_func()->nativeHandle()->current.preedit.cursor_begin;
}

int WInputMethodV2::preeditCursorEnd() const
{
    return d_func()->nativeHandle()->current.preedit.cursor_end;
}
WAYLIB_SERVER_END_NAMESPACE
