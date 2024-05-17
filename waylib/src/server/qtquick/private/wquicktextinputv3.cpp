// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicktextinputv3_p.h"
#include "wquickinputmethodv2_p.h"
#include "wseat.h"
#include "wtoplevelsurface.h"
#include "wtools.h"

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
class WQuickTextInputManagerV3Private : public WObjectPrivate
{
public:
    WQuickTextInputManagerV3Private(WQuickTextInputManagerV3 *qq) :
        WObjectPrivate(qq),
        manager(nullptr)
    {}
    W_DECLARE_PUBLIC(WQuickTextInputManagerV3)

    QWTextInputManagerV3 *manager;
};

WQuickTextInputManagerV3::WQuickTextInputManagerV3(QObject *parent) :
    WQuickWaylandServerInterface(parent),
    WObject(*new WQuickTextInputManagerV3Private(this))
{ }

void WQuickTextInputManagerV3::create()
{
    W_D(WQuickTextInputManagerV3);
    WQuickWaylandServerInterface::create();
    d->manager = QWTextInputManagerV3::create(server()->handle());
    Q_ASSERT(d->manager);
    connect(d->manager, &QWTextInputManagerV3::textInput, this, &WQuickTextInputManagerV3::newTextInput);
}

class WQuickTextInputV3Private : public WQuickTextInputPrivate
{
public:
    W_DECLARE_PUBLIC(WQuickTextInputV3)
    WQuickTextInputV3Private(QWTextInputV3 *h, WQuickTextInputV3 *qq)
        : WQuickTextInputPrivate(qq)
        , handle(h)
    { }

    QWTextInputV3 *const handle;

    wl_client *waylandClient() const override
    {
        return wl_resource_get_client(handle->handle()->resource);
    }
};

WQuickTextInputV3::WQuickTextInputV3(QWTextInputV3 *h, QObject *parent)
    : WQuickTextInput(*new WQuickTextInputV3Private(h, this), parent)
{
    connect(handle(), &QWTextInputV3::enable, this, &WQuickTextInput::enabled);
    connect(handle(), &QWTextInputV3::disable, this, &WQuickTextInput::disabled);
    connect(handle(), &QWTextInputV3::commit, this, &WQuickTextInput::committed);
    connect(handle(), &QWTextInputV3::beforeDestroy, this, &WQuickTextInput::entityAboutToDestroy);
}

WSeat *WQuickTextInputV3::seat() const
{
    W_DC(WQuickTextInputV3);
    return WSeat::fromHandle(QWSeat::from(handle()->handle()->seat));
}

WSurface *WQuickTextInputV3::focusedSurface() const
{
    W_DC(WQuickTextInputV3);
    return WSurface::fromHandle(handle()->handle()->focused_surface);
}

QString WQuickTextInputV3::surroundingText() const
{
    return handle()->handle()->current.surrounding.text;
}

int WQuickTextInputV3::surroundingCursor() const
{
    return handle()->handle()->current.surrounding.cursor;
}

int WQuickTextInputV3::surroundingAnchor() const
{
    return handle()->handle()->current.surrounding.anchor;
}

IME::ChangeCause WQuickTextInputV3::textChangeCause() const
{
    return IME::ChangeCause(handle()->handle()->current.text_change_cause);
}

IME::ContentHints WQuickTextInputV3::contentHints() const
{
    return IME::ContentHints(handle()->handle()->current.content_type.hint);
}

IME::ContentPurpose WQuickTextInputV3::contentPurpose() const
{
    return IME::ContentPurpose(handle()->handle()->current.content_type.purpose);
}

QRect WQuickTextInputV3::cursorRect() const
{
    return WTools::fromWLRBox(&handle()->handle()->current.cursor_rectangle);
}

IME::Features WQuickTextInputV3::features() const
{
    return IME::Features(handle()->handle()->current.features);
}

QWTextInputV3 *WQuickTextInputV3::handle() const
{
    return d_func()->handle;
}

void WQuickTextInputV3::sendEnter(WSurface *surface)
{
    handle()->sendEnter(surface->handle());
}

void WQuickTextInputV3::sendLeave()
{
    if (focusedSurface()) {
        handle()->sendLeave();
    }
}

void WQuickTextInputV3::sendPreeditString(const QString &text, qint32 cursor_begin, qint32 cursor_end)
{
    handle()->sendPreeditString(qPrintable(text), cursor_begin, cursor_end);
}

void WQuickTextInputV3::sendCommitString(const QString &text)
{
    handle()->sendCommitString(qPrintable(text));
}

void WQuickTextInputV3::sendDeleteSurroundingText(quint32 before_length, quint32 after_length)
{
    handle()->sendDeleteSurroundingText(before_length, after_length);
}

void WQuickTextInputV3::sendDone()
{
    handle()->sendDone();
}

void WQuickTextInputV3::handleIMCommitted(WQuickInputMethodV2 *im)
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
