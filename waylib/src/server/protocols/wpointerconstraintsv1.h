// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>

QW_BEGIN_NAMESPACE
class qw_pointer_constraints_v1;
QW_END_NAMESPACE

struct wlr_pointer_constraint_v1;
struct wlr_seat;
struct wlr_surface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WPointerConstraintsV1Private;
class WAYLIB_SERVER_EXPORT WPointerConstraintsV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WPointerConstraintsV1)

public:
    explicit WPointerConstraintsV1();

    QW_NAMESPACE::qw_pointer_constraints_v1 *handle() const;

    QByteArrayView interfaceName() const override;

    wlr_pointer_constraint_v1 *constraintForSurface(wlr_surface *surface, wlr_seat *seat) const;

Q_SIGNALS:
    void newConstraint(wlr_pointer_constraint_v1 *constraint);

protected:
    void create(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
