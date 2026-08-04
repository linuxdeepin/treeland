// Copyright (C) 2023-2026 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputmethodv2_p.h"
#include "wseat.h"
#include "private/wglobal_p.h"

#define delete delete_c
extern "C" {
#include <wlr/types/wlr_input_method_v2.h>
}
#undef delete

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WInputMethodManagerV2Private : public WObjectPrivate
{
public:
    explicit WInputMethodManagerV2Private(WInputMethodManagerV2 *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(WInputMethodManagerV2)

    WNativeListener newInputMethodListener;
};

WInputMethodManagerV2::WInputMethodManagerV2(QObject *parent)
    : QObject(parent)
    , WObject(*new WInputMethodManagerV2Private(this))
{
}

wlr_input_method_manager_v2 *WInputMethodManagerV2::handle() const
{
    return nativeInterface<wlr_input_method_manager_v2>();
}

QByteArrayView WInputMethodManagerV2::interfaceName() const
{
    return "zwp_input_method_manager_v2";
}

void WInputMethodManagerV2::create(WServer *server)
{
    W_D(WInputMethodManagerV2);
    auto *manager = wlr_input_method_manager_v2_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    d->newInputMethodListener.connect(&manager->events.input_method, [this](void *data) {
        Q_EMIT newInputMethod(static_cast<wlr_input_method_v2 *>(data));
    });
}

void WInputMethodManagerV2::destroy([[maybe_unused]] WServer *server)
{
    W_D(WInputMethodManagerV2);
    d->newInputMethodListener.disconnect();
    m_handle = nullptr;
}

wl_global *WInputMethodManagerV2::global() const
{
    return handle() ? handle()->global : nullptr;
}

class Q_DECL_HIDDEN WInputMethodV2Private : public WObjectPrivate
{
public:
    WInputMethodV2Private(wlr_input_method_v2 *handle, WInputMethodV2 *qq)
        : WObjectPrivate(qq)
        , inputMethodHandle(handle)
    {
        Q_ASSERT(inputMethodHandle);
    }

    void connectNativeEvents();
    void instantRelease() override;

    W_DECLARE_PUBLIC(WInputMethodV2)

    wlr_input_method_v2 *inputMethodHandle = nullptr;
    WNativeListener commitListener;
    WNativeListener keyboardGrabListener;
    WNativeListener popupSurfaceListener;
    WNativeListener destroyListener;
};

void WInputMethodV2Private::connectNativeEvents()
{
    W_Q(WInputMethodV2);
    commitListener.connect(&inputMethodHandle->events.commit, [q](void *) {
        Q_EMIT q->committed();
    });
    keyboardGrabListener.connect(&inputMethodHandle->events.grab_keyboard, [q](void *data) {
        Q_EMIT q->newKeyboardGrab(static_cast<wlr_input_method_keyboard_grab_v2 *>(data));
    });
    popupSurfaceListener.connect(&inputMethodHandle->events.new_popup_surface, [q](void *data) {
        Q_EMIT q->newPopupSurface(static_cast<wlr_input_popup_surface_v2 *>(data));
    });
    destroyListener.connect(&inputMethodHandle->events.destroy, [q](void *) {
        q->safeDeleteLater();
    });
}

void WInputMethodV2Private::instantRelease()
{
    commitListener.disconnect();
    keyboardGrabListener.disconnect();
    popupSurfaceListener.disconnect();
    destroyListener.disconnect();
    inputMethodHandle = nullptr;
}

WInputMethodV2::WInputMethodV2(wlr_input_method_v2 *handle, QObject *parent)
    : QObject(parent)
    , WObject(*new WInputMethodV2Private(handle, this))
{
    d_func()->connectNativeEvents();
}

wlr_input_method_v2 *WInputMethodV2::handle() const
{
    return d_func()->inputMethodHandle;
}

WSeat *WInputMethodV2::seat() const
{
    return handle() ? WSeat::fromHandle(handle()->seat) : nullptr;
}

void WInputMethodV2::sendContentType(quint32 hint, quint32 purpose)
{
    wlr_input_method_v2_send_content_type(handle(), hint, purpose);
}

void WInputMethodV2::sendActivate()
{
    wlr_input_method_v2_send_activate(handle());
}

void WInputMethodV2::sendDeactivate()
{
    wlr_input_method_v2_send_deactivate(handle());
}

void WInputMethodV2::sendDone()
{
    wlr_input_method_v2_send_done(handle());
}

void WInputMethodV2::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    const auto utf8 = text.toUtf8();
    wlr_input_method_v2_send_surrounding_text(handle(), utf8.constData(), cursor, anchor);
}

void WInputMethodV2::sendTextChangeCause(quint32 cause)
{
    wlr_input_method_v2_send_text_change_cause(handle(), cause);
}

void WInputMethodV2::sendUnavailable()
{
    wlr_input_method_v2_send_unavailable(handle());
}

QString WInputMethodV2::commitString() const
{
    return QString::fromUtf8(handle()->current.commit_text);
}

uint WInputMethodV2::deleteSurroundingBeforeLength() const
{
    return handle()->current.delete_c.before_length;
}

uint WInputMethodV2::deleteSurroundingAfterLength() const
{
    return handle()->current.delete_c.after_length;
}

QString WInputMethodV2::preeditString() const
{
    return QString::fromUtf8(handle()->current.preedit.text);
}

int WInputMethodV2::preeditCursorBegin() const
{
    return handle()->current.preedit.cursor_begin;
}

int WInputMethodV2::preeditCursorEnd() const
{
    return handle()->current.preedit.cursor_end;
}

WAYLIB_SERVER_END_NAMESPACE
