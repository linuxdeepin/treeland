// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtextinputv3_p.h"
#include "winputmethodv2_p.h"
#include "wseat.h"
#include "wtoplevelsurface.h"
#include "wtools.h"
#include "private/wglobal_p.h"

#include <qwcompositor.h>
#include <qwseat.h>
#include <qwtextinputv3.h>
#include <qwdisplay.h>

#include <QLoggingCategory>
#include <QQmlInfo>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class Q_DECL_HIDDEN WTextInputManagerV3Private : public WObjectPrivate
{
public:
    WTextInputManagerV3Private(WTextInputManagerV3 *qq)
        : WObjectPrivate(qq)
    {}
    W_DECLARE_PUBLIC(WTextInputManagerV3)

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
    auto manager = qw_text_input_manager_v3::create(*server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    connect(manager, &qw_text_input_manager_v3::notify_text_input, this, [this, d](wlr_text_input_v3 *w_text_input_v3){
        auto text_input_v3 = qw_text_input_v3::from(w_text_input_v3);
        auto ti = new WTextInputV3(text_input_v3, this);
        d->textInputs.append(ti);
        Q_EMIT this->newTextInput(ti);
        connect(text_input_v3, &qw_text_input_v3::before_destroy, ti, [ti, d]{
            Q_EMIT ti->entityAboutToDestroy();
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
    return nativeInterface<qw_text_input_manager_v3>()->handle()->global;
}

class Q_DECL_HIDDEN WTextInputV3Private : public WTextInputPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputV3)
    WTextInputV3Private(qw_text_input_v3 *h, WTextInputV3 *qq)
        : WTextInputPrivate(qq)
        , handle(h)
    { }

    qw_text_input_v3 *const handle;

    wl_client *waylandClient() const override
    {
        return wl_resource_get_client(handle->handle()->resource);
    }
};

WTextInputV3::WTextInputV3(qw_text_input_v3 *h, QObject *parent)
    : WTextInput(*new WTextInputV3Private(h, this), parent)
{
    connect(handle(), &qw_text_input_v3::notify_enable, this, &WTextInput::enabled);
    connect(handle(), &qw_text_input_v3::notify_disable, this, &WTextInput::disabled);
    connect(handle(), &qw_text_input_v3::notify_commit, this, &WTextInput::committed);
}

WSeat *WTextInputV3::seat() const
{
    return WSeat::fromHandle(qw_seat::from(handle()->handle()->seat));
}

WSurface *WTextInputV3::focusedSurface() const
{
    return WSurface::fromHandle(handle()->handle()->focused_surface);
}

QString WTextInputV3::surroundingText() const
{
    return handle()->handle()->current.surrounding.text;
}

int WTextInputV3::surroundingCursor() const
{
    return handle()->handle()->current.surrounding.cursor;
}

int WTextInputV3::surroundingAnchor() const
{
    return handle()->handle()->current.surrounding.anchor;
}

IME::ChangeCause WTextInputV3::textChangeCause() const
{
    return IME::ChangeCause(handle()->handle()->current.text_change_cause);
}

IME::ContentHints WTextInputV3::contentHints() const
{
    return IME::ContentHints(handle()->handle()->current.content_type.hint);
}

IME::ContentPurpose WTextInputV3::contentPurpose() const
{
    return IME::ContentPurpose(handle()->handle()->current.content_type.purpose);
}

QRect WTextInputV3::cursorRect() const
{
    return WTools::fromWLRBox(&handle()->handle()->current.cursor_rectangle);
}

IME::Features WTextInputV3::features() const
{
    return IME::Features(handle()->handle()->current.features);
}

qw_text_input_v3 *WTextInputV3::handle() const
{
    return d_func()->handle;
}

void WTextInputV3::sendEnter(WSurface *surface)
{
    handle()->send_enter(surface->handle()->handle());
}

void WTextInputV3::sendLeave()
{
    if (focusedSurface()) {
        handle()->send_leave();
    }
}

void WTextInputV3::sendPreeditString(const QString &text, qint32 cursor_begin, qint32 cursor_end)
{
    handle()->send_preedit_string(qPrintable(text), cursor_begin, cursor_end);
}

void WTextInputV3::sendCommitString(const QString &text)
{
    handle()->send_commit_string(qPrintable(text));
}

void WTextInputV3::sendDeleteSurroundingText(quint32 before_length, quint32 after_length)
{
    handle()->send_delete_surrounding_text(before_length, after_length);
}

void WTextInputV3::sendDone()
{
    handle()->send_done();
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
