// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualkeyboardv1_p.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <wlr/types/wlr_virtual_keyboard_v1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WVirtualKeyboardManagerV1Private : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WVirtualKeyboardManagerV1)
public:
    explicit WVirtualKeyboardManagerV1Private(WVirtualKeyboardManagerV1 *qq)
        : WObjectPrivate(qq)
    { }
    WScopedListener m_newVirtualKeyboardListener;
};

WVirtualKeyboardManagerV1::WVirtualKeyboardManagerV1([[maybe_unused]] QObject *parent)
    : WObject(*new WVirtualKeyboardManagerV1Private(this))
{}

QByteArrayView WVirtualKeyboardManagerV1::interfaceName() const
{
    return "zwp_virtual_keyboard_manager_v1";
}

void WVirtualKeyboardManagerV1::create(WServer *server)
{
    W_D(WVirtualKeyboardManagerV1);
    m_handle = wlr_virtual_keyboard_manager_v1_create(server->handle());
    Q_ASSERT(m_handle);
    auto *manager = static_cast<wlr_virtual_keyboard_manager_v1*>(m_handle);
    d->m_newVirtualKeyboardListener.connect(&manager->events.new_virtual_keyboard, [this](wl_listener *, void *data) {
        Q_EMIT newVirtualKeyboard(static_cast<wlr_virtual_keyboard_v1*>(data));
    });
}

wl_global *WVirtualKeyboardManagerV1::global() const
{
    return static_cast<wlr_virtual_keyboard_manager_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE
