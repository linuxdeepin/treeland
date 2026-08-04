// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrelativepointermanagerv1.h"

#include "wseat.h"

#include "private/wglobal_p.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WRelativePointerManagerV1Private : public WObjectPrivate
{
public:
    explicit WRelativePointerManagerV1Private(WRelativePointerManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline wlr_relative_pointer_manager_v1 *handle() const
    {
        return reinterpret_cast<wlr_relative_pointer_manager_v1 *>(q_func()->m_handle);
    }

    W_DECLARE_PUBLIC(WRelativePointerManagerV1)
};

WRelativePointerManagerV1::WRelativePointerManagerV1()
    : WObject(*new WRelativePointerManagerV1Private(this))
{
}

wlr_relative_pointer_manager_v1 *WRelativePointerManagerV1::handle() const
{
    return reinterpret_cast<wlr_relative_pointer_manager_v1 *>(m_handle);
}

void WRelativePointerManagerV1::sendRelativeMotion(WSeat *seat, uint32_t timeMsec,
                                                   const QPointF &delta,
                                                   const QPointF &unacceleratedDelta)
{
    if (!seat || !seat->handle() || !handle())
        return;

    wlr_relative_pointer_manager_v1_send_relative_motion(
        handle(), seat->handle(), uint64_t(timeMsec) * 1000,
        delta.x(), delta.y(), unacceleratedDelta.x(), unacceleratedDelta.y());
}

QByteArrayView WRelativePointerManagerV1::interfaceName() const
{
    return "zwp_relative_pointer_manager_v1";
}

void WRelativePointerManagerV1::create(WServer *server)
{
    if (!m_handle)
        m_handle = wlr_relative_pointer_manager_v1_create(server->handle());
}

void WRelativePointerManagerV1::destroy([[maybe_unused]] WServer *server)
{
    // wlroots owns the manager until wl_display is destroyed.
    m_handle = nullptr;
}

wl_global *WRelativePointerManagerV1::global() const
{
    W_D(const WRelativePointerManagerV1);
    if (!m_handle)
        return nullptr;

    return d->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
