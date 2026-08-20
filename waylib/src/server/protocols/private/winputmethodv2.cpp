// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodv2_p.h"
#include "wseat.h"
#include "winputdevice.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"
#include "wayliblogging.h"

#include <wlr_all.h>

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

};

WInputMethodManagerV2::WInputMethodManagerV2([[maybe_unused]] QObject *parent)
    : WObject(*new WInputMethodManagerV2Private(this), nullptr)
{
}

QByteArrayView WInputMethodManagerV2::interfaceName() const
{
    return "zwp_input_method_manager_v2";
}

wlr_input_method_manager_v2 *WInputMethodManagerV2::handle() const
{
    return reinterpret_cast<wlr_input_method_manager_v2*>(m_handle);
}

void WInputMethodManagerV2::create(WServer *server)
{
    auto handle = wlr_input_method_manager_v2_create(server->handle());
    Q_ASSERT(handle);
    m_handle = handle;
    W_D(WInputMethodManagerV2);
    listeners()->add(&handle->events.new_input_method, this,
                                   &WInputMethodManagerV2::newInputMethod);
}

void WInputMethodManagerV2::destroy([[maybe_unused]] WServer *server)
{
    // The wlr_input_method_manager_v2 is reclaimed by display.reset() in
    // WServer::stop(); null m_handle so handle()/global() return null instead
    // of a dangling pointer. Manager-owned listeners were already dropped by
    // WServer teardown().
    m_handle = nullptr;
}

wl_global *WInputMethodManagerV2::global() const
{
    if (!m_handle)
        return nullptr;
    return reinterpret_cast<wlr_input_method_manager_v2*>(m_handle)->global;
}

class Q_DECL_HIDDEN WInputMethodV2Private : public WObjectPrivate
{
public:
    WInputMethodV2Private(wlr_input_method_v2 *h, WInputMethodV2 *qq)
        : WObjectPrivate(qq)
        , m_handle(h)
    {
        Q_ASSERT(h);
    }

    inline wlr_input_method_v2 *handle() const {
        return m_handle;
    }

    W_DECLARE_PUBLIC(WInputMethodV2)

private:
    // The helper owns this wrapper and destroys it from the native destroy
    // callback. Keep the address stable until that callback finishes.
    wlr_input_method_v2 *m_handle = nullptr;
};

WInputMethodV2::WInputMethodV2(wlr_input_method_v2 *h) :
    QObject(nullptr),
    WObject(*new WInputMethodV2Private(h, this))
{
    W_D(WInputMethodV2);
    listeners()->add(&h->events.commit, this, &WInputMethodV2::committed);
    listeners()->add(&h->events.grab_keyboard, this, &WInputMethodV2::newKeyboardGrab);
    listeners()->add(&h->events.new_popup_surface, this, &WInputMethodV2::newPopupSurface);
}

WInputMethodV2::~WInputMethodV2()
{
    teardown();
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
    return d_func()->handle()->current._delete.before_length;
}

uint WInputMethodV2::deleteSurroundingAfterLength() const
{
    return d_func()->handle()->current._delete.after_length;
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
