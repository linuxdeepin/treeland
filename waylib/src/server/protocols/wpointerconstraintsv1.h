// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>

QW_BEGIN_NAMESPACE
class qw_pointer_constraint_v1;
class qw_pointer_constraints_v1;
QW_END_NAMESPACE

struct wlr_pointer_constraint_v1;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WSurface;

class WAYLIB_SERVER_EXPORT WPointerConstraintsV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT

public:
    explicit WPointerConstraintsV1();

    QW_NAMESPACE::qw_pointer_constraints_v1 *handle() const;
    QW_NAMESPACE::qw_pointer_constraint_v1 *constraintForSurface(WSurface *surface, const WSeat *seat) const;

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void newConstraint(QW_NAMESPACE::qw_pointer_constraint_v1 *constraint);

protected:
    void create(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
