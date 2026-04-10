// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualpointerv1_p.h"
#include "private/wglobal_p.h"

#include <qwdisplay.h>
#include <qwvirtualpointerv1.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WVirtualPointerManagerV1Private : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WVirtualPointerManagerV1)

public:
    explicit WVirtualPointerManagerV1Private(WVirtualPointerManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }
};

WVirtualPointerManagerV1::WVirtualPointerManagerV1([[maybe_unused]] QObject *parent)
    : WObject(*new WVirtualPointerManagerV1Private(this))
{
}

QByteArrayView WVirtualPointerManagerV1::interfaceName() const
{
    return "zwlr_virtual_pointer_manager_v1";
}

void WVirtualPointerManagerV1::create(WServer *server)
{
    auto manager = qw_virtual_pointer_manager_v1::create(*server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    connect(manager, &qw_virtual_pointer_manager_v1::notify_new_virtual_pointer,
            this, &WVirtualPointerManagerV1::newVirtualPointer);
}

wl_global *WVirtualPointerManagerV1::global() const
{
    return nativeInterface<qw_virtual_pointer_manager_v1>()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
