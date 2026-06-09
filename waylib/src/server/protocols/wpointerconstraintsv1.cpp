// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpointerconstraintsv1.h"

#include "wseat.h"
#include "wsurface.h"

#include "private/wglobal_p.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwpointerconstraintsv1.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WPointerConstraintsV1Private : public WObjectPrivate
{
public:
    explicit WPointerConstraintsV1Private(WPointerConstraintsV1 *qq)
        : WObjectPrivate(qq)
    {
    }
};

WPointerConstraintsV1::WPointerConstraintsV1()
    : WObject(*new WPointerConstraintsV1Private(this))
{
}

qw_pointer_constraints_v1 *WPointerConstraintsV1::handle() const
{
    return nativeInterface<qw_pointer_constraints_v1>();
}

qw_pointer_constraint_v1 *WPointerConstraintsV1::constraintForSurface(WSurface *surface,
                                                                      const WSeat *seat) const
{
    if (!handle() || !surface || !seat)
        return nullptr;

    return qw_pointer_constraint_v1::from(
        handle()->constraint_for_surface(surface->handle()->handle(), seat->nativeHandle()));
}

QByteArrayView WPointerConstraintsV1::interfaceName() const
{
    return "zwp_pointer_constraints_v1";
}

void WPointerConstraintsV1::create(WServer *server)
{
    if (m_handle)
        return;

    m_handle = qw_pointer_constraints_v1::create(*server->handle());
    QObject::connect(handle(), &qw_pointer_constraints_v1::notify_new_constraint,
                     this, [this](wlr_pointer_constraint_v1 *constraint) {
                         Q_EMIT newConstraint(qw_pointer_constraint_v1::from(constraint));
                     });
}

wl_global *WPointerConstraintsV1::global() const
{
    if (!handle())
        return nullptr;

    return handle()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
