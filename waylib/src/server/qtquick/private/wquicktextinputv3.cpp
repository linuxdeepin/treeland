// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicktextinputv3_p.h"
#include "wseat.h"
#include "wtoplevelsurface.h"
#include "wtools.h"

#include <qwcompositor.h>
#include <qwseat.h>
#include <qwtextinputv3.h>

#include <QLoggingCategory>

extern "C" {
#include <wlr/types/wlr_text_input_v3.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class WQuickTextInputManagerV3Private : public WObjectPrivate
{
public:
    WQuickTextInputManagerV3Private(WQuickTextInputManagerV3 *qq) :
        WObjectPrivate(qq),
        manager(nullptr)
    {}
    W_DECLARE_PUBLIC(WQuickTextInputManagerV3)

    QWTextInputManagerV3 *manager;
    QList<WQuickTextInputV3 *> textInputs;
};

WQuickTextInputManagerV3::WQuickTextInputManagerV3(QObject *parent) :
    WQuickWaylandServerInterface(parent),
    WObject(*new WQuickTextInputManagerV3Private(this))
{}

QList<WQuickTextInputV3 *> WQuickTextInputManagerV3::textInputs() const
{
    W_DC(WQuickTextInputManagerV3);
    return d->textInputs;
}

void WQuickTextInputManagerV3::create()
{
    W_D(WQuickTextInputManagerV3);
    WQuickWaylandServerInterface::create();
    d->manager = QWTextInputManagerV3::create(server()->handle());
    connect(d->manager, &QWTextInputManagerV3::textInput, this, [this, d] (QWTextInputV3 *textInput) {
        auto quickTextInput = new WQuickTextInputV3(textInput, this);
        connect(quickTextInput->handle(), &QWTextInputV3::beforeDestroy, this, [d, quickTextInput] () {
            d->textInputs.removeAll(quickTextInput);
            quickTextInput->deleteLater();
        });
        d->textInputs.append(quickTextInput);
        Q_EMIT this->newTextInput(quickTextInput);
    });
}

class WQuickTextInputV3Private : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WQuickTextInputV3)
    WQuickTextInputV3Private(QWTextInputV3 *h, WQuickTextInputV3 *qq)
        : WObjectPrivate(qq)
        , handle(h)
        , state(new WTextInputV3State(&h->handle()->current, qq))
        , pendingState(new WTextInputV3State(&h->handle()->pending, qq))
    { }

    QWTextInputV3 *const handle;
    WTextInputV3State *const state;
    WTextInputV3State *const pendingState;

    wl_client *waylandClient() const override
    {
        return handle->handle()->resource->client;
    }
};

WQuickTextInputV3::WQuickTextInputV3(QWTextInputV3 *handle, QObject *parent) :
    QObject(parent),
    WObject(*new WQuickTextInputV3Private(handle, this), nullptr)
{
    W_D(WQuickTextInputV3);
    connect(d->handle, &QWTextInputV3::enable, this, &WQuickTextInputV3::enable);
    connect(d->handle, &QWTextInputV3::disable, this, &WQuickTextInputV3::disable);
    connect(d->handle, &QWTextInputV3::commit, this, &WQuickTextInputV3::commit);
}

WSeat *WQuickTextInputV3::seat() const
{
    W_DC(WQuickTextInputV3);
    return WSeat::fromHandle(QWSeat::from(d->handle->handle()->seat));
}

WTextInputV3State *WQuickTextInputV3::state() const
{
    W_DC(WQuickTextInputV3);
    return d->state;
}

WTextInputV3State *WQuickTextInputV3::pendingState() const
{
    W_DC(WQuickTextInputV3);
    return d->pendingState;
}

WSurface *WQuickTextInputV3::focusedSurface() const
{
    W_DC(WQuickTextInputV3);
    return WSurface::fromHandle(d->handle->handle()->focused_surface);
}

wl_client *WQuickTextInputV3::waylandClient() const
{
    W_DC(WQuickTextInputV3);
    return d->waylandClient();
}

QWTextInputV3 *WQuickTextInputV3::handle() const
{
    return d_func()->handle;
}

void WQuickTextInputV3::sendEnter(WSurface *surface)
{
    W_D(WQuickTextInputV3);
    d->handle->sendEnter(surface->handle());
}

void WQuickTextInputV3::sendLeave()
{
    W_D(WQuickTextInputV3);
    d->handle->sendLeave();
}

void WQuickTextInputV3::sendPreeditString(const QString &text, qint32 cursor_begin, qint32 cursor_end)
{
    W_D(WQuickTextInputV3);
    d->handle->sendPreeditString(qPrintable(text), cursor_begin, cursor_end);
}

void WQuickTextInputV3::sendCommitString(const QString &text)
{
    W_D(WQuickTextInputV3);
    d->handle->sendCommitString(qPrintable(text));
}

void WQuickTextInputV3::sendDeleteSurroundingText(quint32 before_length, quint32 after_length)
{
    W_D(WQuickTextInputV3);
    d->handle->sendDeleteSurroundingText(before_length, after_length);
}

void WQuickTextInputV3::sendDone()
{
    W_D(WQuickTextInputV3);
    d->handle->sendDone();
}

class WTextInputV3StatePrivate : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WTextInputV3State)
    explicit WTextInputV3StatePrivate(wlr_text_input_v3_state *h, WTextInputV3State *qq)
        : WObjectPrivate(qq)
        , handle(h)
    { }

    wlr_text_input_v3_state *const handle;
};

QString WTextInputV3State::surroundingText() const
{
    W_DC(WTextInputV3State);
    return d->handle->surrounding.text;
}

quint32 WTextInputV3State::surroundingCursor() const
{
    W_DC(WTextInputV3State);
    return d->handle->surrounding.cursor;
}

quint32 WTextInputV3State::surroundingAnchor() const
{
    W_DC(WTextInputV3State);
    return d->handle->surrounding.anchor;
}

WQuickTextInputV3::ChangeCause WTextInputV3State::textChangeCause() const
{
    W_DC(WTextInputV3State);
    return static_cast<WQuickTextInputV3::ChangeCause>(d->handle->text_change_cause);
}

WQuickTextInputV3::ContentHint WTextInputV3State::contentHint() const
{
    W_DC(WTextInputV3State);
    return static_cast<WQuickTextInputV3::ContentHint>(d->handle->content_type.hint);
}

WQuickTextInputV3::ContentPurpose WTextInputV3State::contentPurpose() const
{
    W_DC(WTextInputV3State);
    return static_cast<WQuickTextInputV3::ContentPurpose>(d->handle->content_type.purpose);
}

QRect WTextInputV3State::cursorRect() const
{
    W_DC(WTextInputV3State);
    return WTools::fromWLRBox(&d->handle->cursor_rectangle);
}

WTextInputV3State::Features WTextInputV3State::features() const
{
    W_DC(WTextInputV3State);
    return Features(d->handle->features);
}

WTextInputV3State::WTextInputV3State(wlr_text_input_v3_state *handle, QObject *parent)
    : QObject(parent)
    , WObject(*new WTextInputV3StatePrivate(handle, this))
{ }

QDebug operator<<(QDebug debug, const WTextInputV3State &state)
{
    debug << "SurroundingText: " << state.surroundingText() << Qt::endl;
    debug << "SurroundingCursor: " << state.surroundingCursor() << Qt::endl;
    debug << "SurroundingAnchor: " << state.surroundingAnchor() << Qt::endl;
    debug << "CursorRect: " << state.cursorRect() << Qt::endl;
    debug << "ContentHint: " << state.contentHint() << Qt::endl;
    debug << "ContentPurpose: " << state.contentPurpose() << Qt::endl;
    debug << "Features: " << state.features();
    return debug;
}

WAYLIB_SERVER_END_NAMESPACE
