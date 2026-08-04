// Copyright (C) 2023-2026 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualkeyboardv1_p.h"
#include "private/wglobal_p.h"

extern "C" {
#include <wlr/types/wlr_virtual_keyboard_v1.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WVirtualKeyboardManagerV1Private : public WObjectPrivate
{
public:
    explicit WVirtualKeyboardManagerV1Private(WVirtualKeyboardManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(WVirtualKeyboardManagerV1)

    WNativeListener newVirtualKeyboardListener;
};

WVirtualKeyboardManagerV1::WVirtualKeyboardManagerV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WVirtualKeyboardManagerV1Private(this))
{
}

wlr_virtual_keyboard_manager_v1 *WVirtualKeyboardManagerV1::handle() const
{
    return nativeInterface<wlr_virtual_keyboard_manager_v1>();
}

QByteArrayView WVirtualKeyboardManagerV1::interfaceName() const
{
    return "zwp_virtual_keyboard_manager_v1";
}

void WVirtualKeyboardManagerV1::create(WServer *server)
{
    W_D(WVirtualKeyboardManagerV1);
    auto *manager = wlr_virtual_keyboard_manager_v1_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    d->newVirtualKeyboardListener.connect(&manager->events.new_virtual_keyboard, [this](void *data) {
        Q_EMIT newVirtualKeyboard(static_cast<wlr_virtual_keyboard_v1 *>(data));
    });
}

void WVirtualKeyboardManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WVirtualKeyboardManagerV1);
    d->newVirtualKeyboardListener.disconnect();
    m_handle = nullptr;
}

wl_global *WVirtualKeyboardManagerV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
