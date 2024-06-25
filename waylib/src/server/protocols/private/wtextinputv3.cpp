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

#include <QLoggingCategory>
#include <QQmlInfo>

extern "C" {
#include <wlr/types/wlr_text_input_v3.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class WTextInputManagerV3Private : public WObjectPrivate
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

void WTextInputManagerV3::create(WServer *server)
{
    W_D(WTextInputManagerV3);
    auto manager = QWTextInputManagerV3::create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    connect(manager, &QWTextInputManagerV3::textInput, this, [this, d](QWTextInputV3 *text_input_v3){
        auto ti = new WTextInputV3(text_input_v3, this);
        d->textInputs.append(ti);
        Q_EMIT this->newTextInput(ti);
        connect(text_input_v3, &QWTextInputV3::beforeDestroy, ti, [this, ti, d]{
            Q_EMIT ti->entityAboutToDestroy();
            d->textInputs.removeOne(ti);
            ti->deleteLater();
        });
    });
}

void WTextInputManagerV3::destroy(WServer *server)
{
    for (auto ti : d_func()->textInputs) {
        Q_EMIT ti->entityAboutToDestroy();
        ti->deleteLater();
    }
    d_func()->textInputs.clear();
}

class WTextInputV3Private : public WTextInputPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputV3)
    WTextInputV3Private(QWTextInputV3 *h, WTextInputV3 *qq)
        : WTextInputPrivate(qq)
        , handle(h)
    { }

    QWTextInputV3 *const handle;

    wl_client *waylandClient() const override
    {
        return wl_resource_get_client(handle->handle()->resource);
    }
};

WTextInputV3::WTextInputV3(QWTextInputV3 *h, QObject *parent)
    : WTextInput(*new WTextInputV3Private(h, this), parent)
{
    connect(handle(), &QWTextInputV3::enable, this, &WTextInput::enabled);
    connect(handle(), &QWTextInputV3::disable, this, &WTextInput::disabled);
    connect(handle(), &QWTextInputV3::commit, this, &WTextInput::committed);
}

WSeat *WTextInputV3::seat() const
{
    W_DC(WTextInputV3);
    return WSeat::fromHandle(QWSeat::from(handle()->handle()->seat));
}

WSurface *WTextInputV3::focusedSurface() const
{
    W_DC(WTextInputV3);
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

QWTextInputV3 *WTextInputV3::handle() const
{
    return d_func()->handle;
}

void WTextInputV3::sendEnter(WSurface *surface)
{
    handle()->sendEnter(surface->handle());
}

void WTextInputV3::sendLeave()
{
    if (focusedSurface()) {
        handle()->sendLeave();
    }
}

void WTextInputV3::sendPreeditString(const QString &text, qint32 cursor_begin, qint32 cursor_end)
{
    handle()->sendPreeditString(qPrintable(text), cursor_begin, cursor_end);
}

void WTextInputV3::sendCommitString(const QString &text)
{
    handle()->sendCommitString(qPrintable(text));
}

void WTextInputV3::sendDeleteSurroundingText(quint32 before_length, quint32 after_length)
{
    handle()->sendDeleteSurroundingText(before_length, after_length);
}

void WTextInputV3::sendDone()
{
    handle()->sendDone();
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
