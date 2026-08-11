// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrelativepointerv1.h"
#include "private/wglobal_p.h"

#include <qwrelativepointerv1.h>
#include <qwdisplay.h>

#include <wlr/types/wlr_relative_pointer_v1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::qw_relative_pointer_manager_v1;

class Q_DECL_HIDDEN WRelativePointerManagerV1Private : public WObjectPrivate
{
public:
    WRelativePointerManagerV1Private(WRelativePointerManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline qw_relative_pointer_manager_v1 *handle() const {
        return q_func()->nativeInterface<qw_relative_pointer_manager_v1>();
    }

    inline wlr_relative_pointer_manager_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    W_DECLARE_PUBLIC(WRelativePointerManagerV1)
};

WRelativePointerManagerV1::WRelativePointerManagerV1()
    : WObject(*new WRelativePointerManagerV1Private(this))
{

}

qw_relative_pointer_manager_v1 *WRelativePointerManagerV1::handle() const
{
    return nativeInterface<qw_relative_pointer_manager_v1>();
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

    handle()->send_relative_motion(seat, timeUsec, dx, dy, dxUnaccel, dyUnaccel);
}

void WRelativePointerManagerV1::create(WServer *server)
{
    if (!m_handle) {
        m_handle = qw_relative_pointer_manager_v1::create(*server->handle());
    }
}

wl_global *WRelativePointerManagerV1::global() const
{
    W_D(const WRelativePointerManagerV1);
    if (m_handle)
        return d->nativeHandle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
