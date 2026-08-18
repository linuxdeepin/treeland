// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wpointerconstraintsv1.h>

#include <QList>
#include <QObject>

struct wlr_pointer_constraint_v1;
struct wlr_seat;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WCursor;
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

class Helper;

class PointerConstraintsManager
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WObject
{
    Q_OBJECT

public:
    explicit PointerConstraintsManager(WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *constraints,
                                       QObject *parent = nullptr);
    ~PointerConstraintsManager() override;

    // Deactivate every active constraint (used when leaving Normal mode:
    // lock screen / multitask view / window switch).
    void deactivateAll();

private:
    void onNewConstraint(wlr_pointer_constraint_v1 *constraint);
    bool canActivate(wlr_pointer_constraint_v1 *constraint) const;
    void tryActivateConstraint(wlr_pointer_constraint_v1 *constraint);
    void deactivateConstraint(wlr_pointer_constraint_v1 *constraint);
    void onConstraintDestroyed(wlr_pointer_constraint_v1 *constraint);
    void releaseConstraintCursor(wlr_pointer_constraint_v1 *constraint);
    void onActiveWindowChanged();
    void onSeatGrabChanged(wlr_seat *seat);
    void onPointerFocusChanged(wlr_seat *seat);
    void ensureSeatTracked(wlr_seat *seat);

    WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *m_interface = nullptr;

    QList<wlr_pointer_constraint_v1 *> m_constraints;
    QList<wlr_pointer_constraint_v1 *> m_active;
};
