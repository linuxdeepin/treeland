// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrelativepointerv1.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WRelativePointerManagerV1Private : public WObjectPrivate
{
public:
    WRelativePointerManagerV1Private(WRelativePointerManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline wlr_relative_pointer_manager_v1 *handle() const {
        return reinterpret_cast<wlr_relative_pointer_manager_v1*>(q_func()->m_handle);
    }

    W_DECLARE_PUBLIC(WRelativePointerManagerV1)
};

WRelativePointerManagerV1::WRelativePointerManagerV1()
    : WObject(*new WRelativePointerManagerV1Private(this))
{
}

wlr_relative_pointer_manager_v1 *WRelativePointerManagerV1::handle() const
{
    return reinterpret_cast<wlr_relative_pointer_manager_v1*>(m_handle);
}

QByteArrayView WRelativePointerManagerV1::interfaceName() const
{
    return "zwp_relative_pointer_manager_v1";
}

void WRelativePointerManagerV1::sendRelativeMotion(wlr_seat *seat, uint64_t timeUsec,
                                                    double dx, double dy,
                                                    double dxUnaccel, double dyUnaccel) const
{
    if (!m_handle || !seat)
        return;

    wlr_relative_pointer_manager_v1_send_relative_motion(handle(), seat, timeUsec,
                                                          dx, dy, dxUnaccel, dyUnaccel);
}

void WRelativePointerManagerV1::create(WServer *server)
{
    if (!m_handle) {
        m_handle = wlr_relative_pointer_manager_v1_create(server->handle());
    }
}

void WRelativePointerManagerV1::destroy(WServer *)
{
    // See WPointerConstraintsV1::destroy(): clear m_handle so a restart
    // recreates the global. The native object follows the display lifetime.
    m_handle = nullptr;
}

wl_global *WRelativePointerManagerV1::global() const
{
    W_D(const WRelativePointerManagerV1);
    if (m_handle)
        return d->handle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
