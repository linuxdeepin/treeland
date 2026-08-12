// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualkeyboardv1_p.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"
#include "wayliblogging.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WVirtualKeyboardManagerV1Private : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WVirtualKeyboardManagerV1)
public:
    explicit WVirtualKeyboardManagerV1Private(WVirtualKeyboardManagerV1 *qq)
        : WObjectPrivate(qq)
    { }
};

WVirtualKeyboardManagerV1::WVirtualKeyboardManagerV1([[maybe_unused]] QObject *parent)
    : WObject(*new WVirtualKeyboardManagerV1Private(this))
{}

QByteArrayView WVirtualKeyboardManagerV1::interfaceName() const
{
    return "zwp_virtual_keyboard_manager_v1";
}

wlr_virtual_keyboard_manager_v1 *WVirtualKeyboardManagerV1::handle() const
{
    return reinterpret_cast<wlr_virtual_keyboard_manager_v1*>(m_handle);
}

void WVirtualKeyboardManagerV1::create(WServer *server)
{
    auto manager = wlr_virtual_keyboard_manager_v1_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    W_D(WVirtualKeyboardManagerV1);
    listeners()->add(&manager->events.new_virtual_keyboard, this,
                                       &WVirtualKeyboardManagerV1::newVirtualKeyboard);
}

wl_global *WVirtualKeyboardManagerV1::global() const
{
    return reinterpret_cast<wlr_virtual_keyboard_manager_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE
