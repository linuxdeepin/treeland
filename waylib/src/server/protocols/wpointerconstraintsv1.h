// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WPointerConstraintsV1Private;

// Wayland `zwp_pointer_constraints_v1` wrapper. Creates the wlroots global and
// forwards `new_constraint` to the compositor (treeland) policy layer, which
// decides when to activate/deactivate each constraint. The enforcement itself
// (locked relative motion / confined region clamping) lives in WCursorPrivate.
class WAYLIB_SERVER_EXPORT WPointerConstraintsV1
    : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WPointerConstraintsV1)

public:
    explicit WPointerConstraintsV1();

    QByteArrayView interfaceName() const override;

    // Returns the active constraint for (surface, seat), or nullptr.
    wlr_pointer_constraint_v1 *constraintForSurface(wlr_surface *surface,
                                                    wlr_seat *seat) const;

Q_SIGNALS:
    // Emitted by wlroots whenever a client creates a new locked/confined
    // pointer constraint. The compositor owns the activation policy.
    void newConstraint(wlr_pointer_constraint_v1 *constraint);

protected:
    wlr_pointer_constraints_v1 *handle() const;

    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
