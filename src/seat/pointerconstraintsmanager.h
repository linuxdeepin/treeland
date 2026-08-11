// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wpointerconstraintsv1.h>
#include <wseat.h>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

struct wlr_pointer_constraint_v1;

class Helper;

class PointerConstraintsManager : public QObject
{
    Q_OBJECT

public:
    explicit PointerConstraintsManager(WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *constraints,
                                       Helper *helper, QObject *parent = nullptr);
    ~PointerConstraintsManager() override;

    void deactivateAll();

private:
    void onNewConstraint(wlr_pointer_constraint_v1 *constraint);
    bool canActivate(wlr_pointer_constraint_v1 *constraint) const;
    void activateConstraint(wlr_pointer_constraint_v1 *constraint);
    void deactivateConstraint(wlr_pointer_constraint_v1 *constraint);
    void onConstraintDestroyed(wlr_pointer_constraint_v1 *constraint);
    void onSeatGrabChanged(struct wlr_seat *seat);

    WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *m_interface = nullptr;
    Helper *m_helper = nullptr;

    QSet<wlr_pointer_constraint_v1 *> m_constraints;
    QSet<wlr_pointer_constraint_v1 *> m_active;
    QHash<wlr_pointer_constraint_v1 *, QPointer<WAYLIB_SERVER_NAMESPACE::WCursor>> m_cursors;
    QSet<QW_NAMESPACE::qw_seat *> m_connectedSeats;
};
