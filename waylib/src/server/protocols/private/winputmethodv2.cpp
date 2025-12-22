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
#include <qwdisplay.h>

#include <QKeySequence>
#include <QLoggingCategory>
#include <QRect>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class Q_DECL_HIDDEN WInputMethodManagerV2Private : public WObjectPrivate
{
public:
    explicit WInputMethodManagerV2Private(WInputMethodManagerV2 *qq)
        : WObjectPrivate(qq)
    {}
    W_DECLARE_PUBLIC(WInputMethodManagerV2)

    QList<WInputMethodV2 *> inputMethods;
};

WInputMethodManagerV2::WInputMethodManagerV2([[maybe_unused]] QObject *parent)
    : WObject(*new WInputMethodManagerV2Private(this), nullptr)
{ 
}

QByteArrayView WInputMethodManagerV2::interfaceName() const
{
    return "zwp_input_method_manager_v2";
}

void WInputMethodManagerV2::create(WServer *server)
{
    auto handle = qw_input_method_manager_v2::create(*server->handle());
    Q_ASSERT(handle);
    m_handle = handle;
    connect(handle, &qw_input_method_manager_v2::notify_input_method, this, [this](wlr_input_method_v2* im) {
        Q_EMIT newInputMethod(qw_input_method_v2::from(im));
    });
}

wl_global *WInputMethodManagerV2::global() const
{
    return nativeInterface<qw_input_method_manager_v2>()->handle()->global;
}

class Q_DECL_HIDDEN WInputMethodV2Private : public WWrapObjectPrivate
{
public:
    WInputMethodV2Private(qw_input_method_v2 *h, WInputMethodV2 *qq)
        : WWrapObjectPrivate(qq)
    {
        initHandle(h);
    }

    WWRAP_HANDLE_FUNCTIONS(qw_input_method_v2, wlr_input_method_v2)

    W_DECLARE_PUBLIC(WInputMethodV2)
};

WInputMethodV2::WInputMethodV2(qw_input_method_v2 *h, QObject *parent) :
    WWrapObject(*new WInputMethodV2Private(h, this), parent)
{
    connect(handle(), &qw_input_method_v2::notify_commit, this, &WInputMethodV2::committed);
    connect(handle(), &qw_input_method_v2::notify_grab_keyboard, this, [this](wlr_input_method_keyboard_grab_v2 *grab) {
        Q_EMIT newKeyboardGrab(qw_input_method_keyboard_grab_v2::from(grab));
    });
    connect(handle(), &qw_input_method_v2::notify_new_popup_surface, this, [this](wlr_input_popup_surface_v2 *surface) {
        Q_EMIT newPopupSurface(qw_input_popup_surface_v2::from(surface));
    });
}

qw_input_method_v2 *WInputMethodV2::handle() const
{
    return d_func()->handle();
}

WSeat *WInputMethodV2::seat() const
{
    W_DC(WInputMethodV2);
    return WSeat::fromHandle(qw_seat::from(d->nativeHandle()->seat));
}

void WInputMethodV2::sendContentType(quint32 hint, quint32 purpose)
{
    W_D(WInputMethodV2);
    d->handle()->send_content_type(hint, purpose);
}

void WInputMethodV2::sendActivate()
{
    W_D(WInputMethodV2);
    d->handle()->send_activate();
}

void WInputMethodV2::sendDeactivate()
{
    W_D(WInputMethodV2);
    d->handle()->send_deactivate();
}

void WInputMethodV2::sendDone()
{
    W_D(WInputMethodV2);
    d->handle()->send_done();
}

void WInputMethodV2::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    W_D(WInputMethodV2);
    d->handle()->send_surrounding_text(qPrintable(text), cursor, anchor);
}

void WInputMethodV2::sendTextChangeCause(quint32 cause)
{
    W_D(WInputMethodV2);
    d->handle()->send_text_change_cause(cause);
}

void WInputMethodV2::sendUnavailable()
{
    W_D(WInputMethodV2);
    d->handle()->send_unavailable();
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
