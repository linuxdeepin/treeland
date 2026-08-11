// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpointerconstraintsv1.h"
#include "private/wglobal_p.h"

#include <qwpointerconstraintsv1.h>
#include <qwdisplay.h>

#include <wlr/types/wlr_pointer_constraints_v1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::qw_pointer_constraints_v1;

class Q_DECL_HIDDEN WPointerConstraintsV1Private : public WObjectPrivate
{
public:
    WPointerConstraintsV1Private(WPointerConstraintsV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline qw_pointer_constraints_v1 *handle() const {
        return q_func()->nativeInterface<qw_pointer_constraints_v1>();
    }

    inline wlr_pointer_constraints_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    W_DECLARE_PUBLIC(WPointerConstraintsV1)
};

WPointerConstraintsV1::WPointerConstraintsV1()
    : WObject(*new WPointerConstraintsV1Private(this))
{

}

qw_pointer_constraints_v1 *WPointerConstraintsV1::handle() const
{
    return nativeInterface<qw_pointer_constraints_v1>();
}

QByteArrayView WPointerConstraintsV1::interfaceName() const
{
    return "zwp_pointer_constraints_v1";
}

wlr_pointer_constraint_v1 *WPointerConstraintsV1::constraintForSurface(wlr_surface *surface, wlr_seat *seat) const
{
    if (!m_handle)
        return nullptr;

    return handle()->constraint_for_surface(surface, seat);
}

void WPointerConstraintsV1::create(WServer *server)
{
    if (!m_handle) {
        m_handle = qw_pointer_constraints_v1::create(*server->handle());
        QObject::connect(handle(), &qw_pointer_constraints_v1::notify_new_constraint, this, [this] (wlr_pointer_constraint_v1 *constraint) {
            Q_EMIT newConstraint(constraint);
        });
    }
}

wl_global *WPointerConstraintsV1::global() const
{
    W_D(const WPointerConstraintsV1);
    if (m_handle)
        return d->nativeHandle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
