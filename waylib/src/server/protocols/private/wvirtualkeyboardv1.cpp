// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualkeyboardv1_p.h"
#include "private/wglobal_p.h"

#include <qwvirtualkeyboardv1.h>
#include <qwdisplay.h>

#include <QLoggingCategory>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)

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

void WVirtualKeyboardManagerV1::create(WServer *server)
{
    auto manager = qw_virtual_keyboard_manager_v1::create(*server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    connect(manager, &qw_virtual_keyboard_manager_v1::notify_new_virtual_keyboard, this, &WVirtualKeyboardManagerV1::newVirtualKeyboard);
}

wl_global *WVirtualKeyboardManagerV1::global() const
{
    return nativeInterface<qw_virtual_keyboard_manager_v1>()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
