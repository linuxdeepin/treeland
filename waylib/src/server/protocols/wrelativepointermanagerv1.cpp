// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrelativepointermanagerv1.h"

#include "wseat.h"

#include "private/wglobal_p.h"

#include <qwdisplay.h>
#include <qwrelativepointerv1.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WRelativePointerManagerV1Private : public WObjectPrivate
{
public:
    explicit WRelativePointerManagerV1Private(WRelativePointerManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }
};

WRelativePointerManagerV1::WRelativePointerManagerV1()
    : WObject(*new WRelativePointerManagerV1Private(this))
{
}

qw_relative_pointer_manager_v1 *WRelativePointerManagerV1::handle() const
{
    return nativeInterface<qw_relative_pointer_manager_v1>();
}

void WRelativePointerManagerV1::sendRelativeMotion(WSeat *seat, uint32_t timeMsec,
                                                   const QPointF &delta,
                                                   const QPointF &unacceleratedDelta)
{
    if (!seat || !handle())
        return;

    handle()->send_relative_motion(seat->nativeHandle(), uint64_t(timeMsec) * 1000,
                                   delta.x(), delta.y(),
                                   unacceleratedDelta.x(), unacceleratedDelta.y());
}

QByteArrayView WRelativePointerManagerV1::interfaceName() const
{
    return "zwp_relative_pointer_manager_v1";
}

void WRelativePointerManagerV1::create(WServer *server)
{
    if (!m_handle)
        m_handle = qw_relative_pointer_manager_v1::create(*server->handle());
}

wl_global *WRelativePointerManagerV1::global() const
{
    if (!handle())
        return nullptr;

    return handle()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
