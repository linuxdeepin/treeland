// Copyright (C) 2023-2026 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtextinputv3_p.h"
#include "winputmethodv2_p.h"
#include "wseat.h"
#include "wtoplevelsurface.h"
#include "wtools.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_compositor.h>

#include <QQmlInfo>

WAYLIB_SERVER_BEGIN_NAMESPACE
class Q_DECL_HIDDEN WTextInputManagerV3Private : public WObjectPrivate
{
public:
    WTextInputManagerV3Private(WTextInputManagerV3 *qq)
        : WObjectPrivate(qq)
    {}
    W_DECLARE_PUBLIC(WTextInputManagerV3)

    WScopedListener m_newTextInputListener;
    QList<WTextInputV3 *> textInputs;
};

WTextInputManagerV3::WTextInputManagerV3(QObject *parent)
    : QObject(parent)
    , WObject(*new WTextInputManagerV3Private(this))
{ }


QByteArrayView WTextInputManagerV3::interfaceName() const
{
    return "zwp_text_input_manager_v3";
}

void WTextInputManagerV3::create(WServer *server)
{
    W_D(WTextInputManagerV3);
    m_handle = wlr_text_input_manager_v3_create(server->handle());
    Q_ASSERT(m_handle);
    auto *manager = static_cast<wlr_text_input_manager_v3*>(m_handle);
    d->m_newTextInputListener.connect(&manager->events.text_input, [this, d](wl_listener *, void *data) {
        auto *w_text_input_v3 = static_cast<wlr_text_input_v3*>(data);
        auto ti = new WTextInputV3(w_text_input_v3, this);
        d->textInputs.append(ti);
        Q_EMIT this->newTextInput(ti);
        connect(ti, &WTextInputV3::entityAboutToDestroy, ti, [ti, d] {
            d->textInputs.removeOne(ti);
            ti->deleteLater();
        });
    });
}

void WTextInputManagerV3::destroy([[maybe_unused]] WServer *server)
{
    for (auto ti : std::as_const(d_func()->textInputs)) {
        Q_EMIT ti->entityAboutToDestroy();
        ti->deleteLater();
    }
    d_func()->textInputs.clear();
}

wl_global *WTextInputManagerV3::global() const
{
    return static_cast<wlr_text_input_manager_v3*>(m_handle)->global;
}

class Q_DECL_HIDDEN WTextInputV3Private : public WTextInputPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputV3)
    WTextInputV3Private(wlr_text_input_v3 *h, WTextInputV3 *qq)
        : WTextInputPrivate(qq)
        , handle(h)
    { }

    wlr_text_input_v3 *const handle;
    WScopedListener m_enableListener;
    WScopedListener m_disableListener;
    WScopedListener m_commitListener;
    WScopedListener m_destroyListener;

    wl_client *waylandClient() const override
    {
        return wl_resource_get_client(handle->resource);
    }
};

WTextInputV3::WTextInputV3(wlr_text_input_v3 *h, QObject *parent)
    : WTextInput(*new WTextInputV3Private(h, this), parent)
{
    W_D(WTextInputV3);
    d->m_enableListener.connect(&h->events.enable, [this](wl_listener *, void *) {
        Q_EMIT enabled();
    });
    d->m_disableListener.connect(&h->events.disable, [this](wl_listener *, void *) {
        Q_EMIT disabled();
    });
    d->m_commitListener.connect(&h->events.commit, [this](wl_listener *, void *) {
        Q_EMIT committed();
    });
    d->m_destroyListener.connect(&h->events.destroy, [this](wl_listener *, void *) {
        Q_EMIT entityAboutToDestroy();
    });
}

WSeat *WTextInputV3::seat() const
{
    return WSeat::fromHandle(handle()->seat);
}

WSurface *WTextInputV3::focusedSurface() const
{
    return WSurface::fromHandle(handle()->focused_surface);
}

QString WTextInputV3::surroundingText() const
{
    return handle()->current.surrounding.text;
}

int WTextInputV3::surroundingCursor() const
{
    return handle()->current.surrounding.cursor;
}

int WTextInputV3::surroundingAnchor() const
{
    return handle()->current.surrounding.anchor;
}

IME::ChangeCause WTextInputV3::textChangeCause() const
{
    return IME::ChangeCause(handle()->current.text_change_cause);
}

IME::ContentHints WTextInputV3::contentHints() const
{
    return IME::ContentHints(handle()->current.content_type.hint);
}

IME::ContentPurpose WTextInputV3::contentPurpose() const
{
    return IME::ContentPurpose(handle()->current.content_type.purpose);
}

QRect WTextInputV3::cursorRect() const
{
    return WTools::fromWLRBox(&handle()->current.cursor_rectangle);
}

IME::Features WTextInputV3::features() const
{
    return IME::Features(handle()->current.features);
}

wlr_text_input_v3 *WTextInputV3::handle() const
{
    return d_func()->handle;
}

void WTextInputV3::sendEnter(WSurface *surface)
{
    Q_ASSERT(surface);
    if (!surface)
        return;

    auto *targetSurface = surface->handle();
    auto *focusedSurface = handle()->focused_surface;
    if (focusedSurface == targetSurface)
        return;

    if (focusedSurface)
        wlr_text_input_v3_send_leave(d_func()->handle);

    wlr_text_input_v3_send_enter(d_func()->handle, targetSurface);
}

void WTextInputV3::sendLeave()
{
    if (handle()->focused_surface) {
        wlr_text_input_v3_send_leave(d_func()->handle);
    }
}

void WTextInputV3::sendPreeditString(const QString &text, qint32 cursor_begin, qint32 cursor_end)
{
    wlr_text_input_v3_send_preedit_string(d_func()->handle, qPrintable(text), cursor_begin, cursor_end);
}

void WTextInputV3::sendCommitString(const QString &text)
{
    wlr_text_input_v3_send_commit_string(d_func()->handle, qPrintable(text));
}

void WTextInputV3::sendDeleteSurroundingText(quint32 before_length, quint32 after_length)
{
    wlr_text_input_v3_send_delete_surrounding_text(d_func()->handle, before_length, after_length);
}

void WTextInputV3::sendDone()
{
    wlr_text_input_v3_send_done(d_func()->handle);
}

void WTextInputV3::handleIMCommitted(WInputMethodV2 *im)
{
    if (!im->preeditString().isEmpty()) {
        sendPreeditString(im->preeditString(), im->preeditCursorBegin(), im->preeditCursorEnd());
    }
    if (!im->commitString().isEmpty()) {
        sendCommitString(im->commitString());
    }
    if (im->deleteSurroundingBeforeLength() || im->deleteSurroundingAfterLength()) {
        sendDeleteSurroundingText(im->deleteSurroundingBeforeLength(), im->deleteSurroundingAfterLength());
    }
    sendDone();
}
WAYLIB_SERVER_END_NAMESPACE
