// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodv2_p.h"
#include "wseat.h"
#include "winputdevice.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#define delete wlr_delete
#include <wlr/types/wlr_input_method_v2.h>
#undef delete
#include <wlr/types/wlr_compositor.h>

#include <QKeySequence>
#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE
class Q_DECL_HIDDEN WInputMethodManagerV2Private : public WObjectPrivate
{
public:
    explicit WInputMethodManagerV2Private(WInputMethodManagerV2 *qq)
        : WObjectPrivate(qq)
    {}
    W_DECLARE_PUBLIC(WInputMethodManagerV2)

    WScopedListener m_inputMethodListener;
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
    W_D(WInputMethodManagerV2);
    m_handle = wlr_input_method_manager_v2_create(server->handle());
    Q_ASSERT(m_handle);
    auto *manager = static_cast<wlr_input_method_manager_v2*>(m_handle);
    d->m_inputMethodListener.connect(&manager->events.input_method, [this](wl_listener *, void *data) {
        Q_EMIT newInputMethod(static_cast<wlr_input_method_v2*>(data));
    });
}

wl_global *WInputMethodManagerV2::global() const
{
    return static_cast<wlr_input_method_manager_v2*>(m_handle)->global;
}

class Q_DECL_HIDDEN WInputMethodV2Private : public WWrapObjectPrivate
{
public:
    WInputMethodV2Private(wlr_input_method_v2 *h, WInputMethodV2 *qq)
        : WWrapObjectPrivate(qq)
    {
        initNativeHandle(h, &h->events.destroy);
    }

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_input_method_v2)

    WScopedListener m_commitListener;
    WScopedListener m_grabKeyboardListener;
    WScopedListener m_newPopupSurfaceListener;

    W_DECLARE_PUBLIC(WInputMethodV2)
};

WInputMethodV2::WInputMethodV2(wlr_input_method_v2 *h, QObject *parent) :
    WWrapObject(*new WInputMethodV2Private(h, this), parent)
{
    W_D(WInputMethodV2);
    d->m_commitListener.connect(&h->events.commit, [this](wl_listener *, void *) {
        Q_EMIT committed();
    });
    d->m_grabKeyboardListener.connect(&h->events.grab_keyboard, [this](wl_listener *, void *data) {
        Q_EMIT newKeyboardGrab(static_cast<wlr_input_method_keyboard_grab_v2*>(data));
    });
    d->m_newPopupSurfaceListener.connect(&h->events.new_popup_surface, [this](wl_listener *, void *data) {
        Q_EMIT newPopupSurface(static_cast<wlr_input_popup_surface_v2*>(data));
    });
}

wlr_input_method_v2 *WInputMethodV2::handle() const
{
    return d_func()->handle();
}

WSeat *WInputMethodV2::seat() const
{
    W_DC(WInputMethodV2);
    return WSeat::fromHandle(d->handle()->seat);
}

void WInputMethodV2::sendContentType(quint32 hint, quint32 purpose)
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_content_type(d->handle(), hint, purpose);
}

void WInputMethodV2::sendActivate()
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_activate(d->handle());
}

void WInputMethodV2::sendDeactivate()
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_deactivate(d->handle());
}

void WInputMethodV2::sendDone()
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_done(d->handle());
}

void WInputMethodV2::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_surrounding_text(d->handle(), qPrintable(text), cursor, anchor);
}

void WInputMethodV2::sendTextChangeCause(quint32 cause)
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_text_change_cause(d->handle(), cause);
}

void WInputMethodV2::sendUnavailable()
{
    W_D(WInputMethodV2);
    wlr_input_method_v2_send_unavailable(d->handle());
}

QString WInputMethodV2::commitString() const
{
    return d_func()->handle()->current.commit_text;
}

uint WInputMethodV2::deleteSurroundingBeforeLength() const
{
    return d_func()->handle()->current.wlr_delete.before_length;
}

uint WInputMethodV2::deleteSurroundingAfterLength() const
{
    return d_func()->handle()->current.wlr_delete.after_length;
}

QString WInputMethodV2::preeditString() const
{
    return d_func()->handle()->current.preedit.text;
}

int WInputMethodV2::preeditCursorBegin() const
{
    return d_func()->handle()->current.preedit.cursor_begin;
}

int WInputMethodV2::preeditCursorEnd() const
{
    return d_func()->handle()->current.preedit.cursor_end;
}
WAYLIB_SERVER_END_NAMESPACE
