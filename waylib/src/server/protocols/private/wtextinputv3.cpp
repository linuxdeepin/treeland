// Copyright (C) 2023-2026 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtextinputv3_p.h"
#include "winputmethodv2_p.h"
#include "wseat.h"
#include "wsurface.h"
#include "wtools.h"
#include "private/wglobal_p.h"

extern "C" {
#include <wlr/types/wlr_text_input_v3.h>
}

#include <utility>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WTextInputManagerV3Private : public WObjectPrivate
{
public:
    explicit WTextInputManagerV3Private(WTextInputManagerV3 *qq)
        : WObjectPrivate(qq)
    {
    }

    void onNewTextInput(wlr_text_input_v3 *nativeTextInput)
    {
        W_Q(WTextInputManagerV3);
        auto *textInput = new WTextInputV3(nativeTextInput, q);
        textInputs.append(textInput);
        QObject::connect(textInput, &WTextInput::entityAboutToDestroy, q, [this, textInput] {
            textInputs.removeOne(textInput);
        });
        Q_EMIT q->newTextInput(textInput);
    }

    W_DECLARE_PUBLIC(WTextInputManagerV3)

    QList<WTextInputV3 *> textInputs;
    WNativeListener newTextInputListener;
};

WTextInputManagerV3::WTextInputManagerV3(QObject *parent)
    : QObject(parent)
    , WObject(*new WTextInputManagerV3Private(this))
{
}

wlr_text_input_manager_v3 *WTextInputManagerV3::handle() const
{
    return nativeInterface<wlr_text_input_manager_v3>();
}

QByteArrayView WTextInputManagerV3::interfaceName() const
{
    return "zwp_text_input_manager_v3";
}

void WTextInputManagerV3::create(WServer *server)
{
    W_D(WTextInputManagerV3);
    auto *manager = wlr_text_input_manager_v3_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    d->newTextInputListener.connect(&manager->events.text_input, [d](void *data) {
        d->onNewTextInput(static_cast<wlr_text_input_v3 *>(data));
    });
}

void WTextInputManagerV3::destroy([[maybe_unused]] WServer *server)
{
    W_D(WTextInputManagerV3);
    d->newTextInputListener.disconnect();
    const auto textInputs = std::exchange(d->textInputs, {});
    for (auto *textInput : textInputs) {
        textInput->release();
        textInput->deleteLater();
    }
    m_handle = nullptr;
}

wl_global *WTextInputManagerV3::global() const
{
    return handle() ? handle()->global : nullptr;
}

class Q_DECL_HIDDEN WTextInputV3Private : public WTextInputPrivate
{
public:
    WTextInputV3Private(wlr_text_input_v3 *handle, WTextInputV3 *qq)
        : WTextInputPrivate(qq)
        , textInputHandle(handle)
    {
        Q_ASSERT(textInputHandle);
    }

    ~WTextInputV3Private() override
    {
        disconnectNativeEvents();
    }

    void connectNativeEvents();
    void disconnectNativeEvents();
    void release(bool notify);

    wl_client *waylandClient() const override
    {
        return textInputHandle ? wl_resource_get_client(textInputHandle->resource) : nullptr;
    }

    W_DECLARE_PUBLIC(WTextInputV3)

    wlr_text_input_v3 *textInputHandle = nullptr;
    WNativeListener enableListener;
    WNativeListener disableListener;
    WNativeListener commitListener;
    WNativeListener destroyListener;
};

void WTextInputV3Private::connectNativeEvents()
{
    W_Q(WTextInputV3);
    enableListener.connect(&textInputHandle->events.enable, [q](void *) {
        Q_EMIT q->enabled();
    });
    disableListener.connect(&textInputHandle->events.disable, [q](void *) {
        Q_EMIT q->disabled();
    });
    commitListener.connect(&textInputHandle->events.commit, [q](void *) {
        Q_EMIT q->committed();
    });
    destroyListener.connect(&textInputHandle->events.destroy, [this, q](void *) {
        release(true);
        q->deleteLater();
    });
}

void WTextInputV3Private::disconnectNativeEvents()
{
    enableListener.disconnect();
    disableListener.disconnect();
    commitListener.disconnect();
    destroyListener.disconnect();
}

void WTextInputV3Private::release(bool notify)
{
    if (!textInputHandle)
        return;
    disconnectNativeEvents();
    textInputHandle = nullptr;
    if (notify)
        Q_EMIT q_func()->entityAboutToDestroy();
}

WTextInputV3::WTextInputV3(wlr_text_input_v3 *handle, QObject *parent)
    : WTextInput(*new WTextInputV3Private(handle, this), parent)
{
    d_func()->connectNativeEvents();
}

WTextInputV3::~WTextInputV3()
{
    d_func()->release(false);
}

void WTextInputV3::release()
{
    d_func()->release(true);
}

wlr_text_input_v3 *WTextInputV3::handle() const
{
    return d_func()->textInputHandle;
}

WSeat *WTextInputV3::seat() const
{
    return handle() ? WSeat::fromHandle(handle()->seat) : nullptr;
}

WSurface *WTextInputV3::focusedSurface() const
{
    return handle() ? WSurface::fromHandle(handle()->focused_surface) : nullptr;
}

QString WTextInputV3::surroundingText() const
{
    return QString::fromUtf8(handle()->current.surrounding.text);
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

void WTextInputV3::sendEnter(WSurface *surface)
{
    Q_ASSERT(surface);
    if (!surface || !handle())
        return;

    auto *targetSurface = surface->handle();
    if (handle()->focused_surface == targetSurface)
        return;
    if (handle()->focused_surface)
        wlr_text_input_v3_send_leave(handle());
    wlr_text_input_v3_send_enter(handle(), targetSurface);
}

void WTextInputV3::sendLeave()
{
    if (handle() && handle()->focused_surface)
        wlr_text_input_v3_send_leave(handle());
}

void WTextInputV3::sendPreeditString(const QString &text, qint32 cursorBegin, qint32 cursorEnd)
{
    const auto utf8 = text.toUtf8();
    wlr_text_input_v3_send_preedit_string(handle(), utf8.constData(), cursorBegin, cursorEnd);
}

void WTextInputV3::sendCommitString(const QString &text)
{
    const auto utf8 = text.toUtf8();
    wlr_text_input_v3_send_commit_string(handle(), utf8.constData());
}

void WTextInputV3::sendDeleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    wlr_text_input_v3_send_delete_surrounding_text(handle(), beforeLength, afterLength);
}

void WTextInputV3::sendDone()
{
    wlr_text_input_v3_send_done(handle());
}

void WTextInputV3::handleIMCommitted(WInputMethodV2 *im)
{
    if (!im->preeditString().isEmpty())
        sendPreeditString(im->preeditString(), im->preeditCursorBegin(), im->preeditCursorEnd());
    if (!im->commitString().isEmpty())
        sendCommitString(im->commitString());
    if (im->deleteSurroundingBeforeLength() || im->deleteSurroundingAfterLength())
        sendDeleteSurroundingText(im->deleteSurroundingBeforeLength(), im->deleteSurroundingAfterLength());
    sendDone();
}

WAYLIB_SERVER_END_NAMESPACE
