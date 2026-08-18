// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpointerconstraintsv1.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WPointerConstraintsV1Private : public WObjectPrivate
{
public:
    WPointerConstraintsV1Private(WPointerConstraintsV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline wlr_pointer_constraints_v1 *handle() const {
        return reinterpret_cast<wlr_pointer_constraints_v1*>(q_func()->m_handle);
    }

    W_DECLARE_PUBLIC(WPointerConstraintsV1)
};

WPointerConstraintsV1::WPointerConstraintsV1()
    : WObject(*new WPointerConstraintsV1Private(this))
{
}

wlr_pointer_constraints_v1 *WPointerConstraintsV1::handle() const
{
    return reinterpret_cast<wlr_pointer_constraints_v1*>(m_handle);
}

QByteArrayView WPointerConstraintsV1::interfaceName() const
{
    return "zwp_pointer_constraints_v1";
}

wlr_pointer_constraint_v1 *WPointerConstraintsV1::constraintForSurface(wlr_surface *surface,
                                                                       wlr_seat *seat) const
{
    if (!m_handle)
        return nullptr;

    return wlr_pointer_constraints_v1_constraint_for_surface(handle(), surface, seat);
}

void WPointerConstraintsV1::create(WServer *server)
{
    if (!m_handle) {
        m_handle = wlr_pointer_constraints_v1_create(server->handle());
        listeners()->add(&handle()->events.new_constraint, this,
                         [this](wlr_pointer_constraint_v1 *constraint) {
                             Q_EMIT newConstraint(constraint);
                         });
    }
}

void WPointerConstraintsV1::destroy(WServer *)
{
    // create() is guarded by `if (!m_handle)`, so the handle must be cleared
    // here — otherwise restart would skip recreating the global. The native
    // object itself has no public destroy() function and is reclaimed when
    // the display is destroyed in WServer::stop().
    m_handle = nullptr;
}

wl_global *WPointerConstraintsV1::global() const
{
    W_D(const WPointerConstraintsV1);
    if (m_handle)
        return d->handle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
