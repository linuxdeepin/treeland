// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpointerconstraintsv1.h"

#include "wseat.h"
#include "wsurface.h"

#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwpointerconstraintsv1.h>

#include <sys/types.h>
#include <wayland-server-core.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

static const char *constraintTypeName(wlr_pointer_constraint_v1 *constraint)
{
    if (!constraint)
        return "unknown";

    switch (constraint->type) {
    case WLR_POINTER_CONSTRAINT_V1_LOCKED:
        return "locked";
    case WLR_POINTER_CONSTRAINT_V1_CONFINED:
        return "confined";
    }

    return "unknown";
}

static const char *constraintLifetimeName(uint32_t lifetime)
{
    switch (lifetime) {
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT:
        return "oneshot";
    case ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT:
        return "persistent";
    }

    return "unknown";
}

static qint64 constraintClientPid(wlr_pointer_constraint_v1 *constraint)
{
    if (!constraint || !constraint->resource)
        return -1;

    pid_t pid = -1;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(wl_resource_get_client(constraint->resource), &pid, &uid, &gid);
    return pid;
}

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
                         if (!constraint) {
                             qCWarning(lcWlSeat)
                                 << "Ignoring null pointer constraint notification";
                             return;
                         }

                         qCInfo(lcWlSeat)
                             << "New pointer constraint"
                             << "type =" << constraintTypeName(constraint)
                             << "lifetime =" << constraintLifetimeName(constraint->lifetime)
                             << "constraint =" << constraint
                             << "surface =" << constraint->surface
                             << "seat =" << constraint->seat
                             << "clientPid =" << constraintClientPid(constraint);
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
