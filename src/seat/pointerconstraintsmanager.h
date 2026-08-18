// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wpointerconstraintsv1.h>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <map>
#include <memory>

struct wlr_pointer_constraint_v1;
struct wlr_seat;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WCursor;
class WSeat;
class WListenerOwner;
WAYLIB_SERVER_END_NAMESPACE

class Helper;

// Owns the compositor-side activation/deactivation policy for
// zwp_pointer_constraints_v1. Created by Helper; waylib's WCursor only
// provides the enforcement mechanism (setActivePointerConstraint), this
// class decides when to call it based on pointer focus, grabs and the
// current compositor mode.
class PointerConstraintsManager : public QObject
{
    Q_OBJECT

public:
    explicit PointerConstraintsManager(WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *constraints,
                                       Helper *helper, QObject *parent = nullptr);
    ~PointerConstraintsManager() override;

    // Deactivate every active constraint (used when leaving Normal mode:
    // lock screen / multitask view / window switch).
    void deactivateAll();

private:
    void onNewConstraint(wlr_pointer_constraint_v1 *constraint);
    bool canActivate(wlr_pointer_constraint_v1 *constraint) const;
    void activateConstraint(wlr_pointer_constraint_v1 *constraint);
    void deactivateConstraint(wlr_pointer_constraint_v1 *constraint);
    void onConstraintDestroyed(wlr_pointer_constraint_v1 *constraint);
    void onSeatGrabChanged(wlr_seat *seat);
    void onPointerFocusChanged(wlr_seat *seat);
    void ensureSeatTracked(wlr_seat *seat);

    WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *m_interface = nullptr;
    Helper *m_helper = nullptr;

    QSet<wlr_pointer_constraint_v1 *> m_constraints;
    QSet<wlr_pointer_constraint_v1 *> m_active;
    QHash<wlr_pointer_constraint_v1 *, QPointer<WAYLIB_SERVER_NAMESPACE::WCursor>> m_cursors;

    // Per-constraint and per-seat WListenerOwner: each object's wl_signal
    // listeners (constraint set_region/destroy; seat pointer_grab_begin/end,
    // pointer_state focus_change, destroy) live on its own owner. When the
    // object is destroyed, erasing the owner -> ~WListenerOwner -> teardown
    // disconnects those listeners *before* wlroots runs
    // assert(wl_list_empty(...)) and free()s the object. A single shared
    // owner would leave wl_listener nodes in freed memory (debug abort /
    // release use-after-free), so each object must own its listeners.
    // std::map (not QHash): the unique_ptr value is move-only and QHash's
    // copy-on-write detach would try to copy it.
    std::map<wlr_pointer_constraint_v1 *, std::unique_ptr<WAYLIB_SERVER_NAMESPACE::WListenerOwner>> m_constraintOwners;
    std::map<wlr_seat *, std::unique_ptr<WAYLIB_SERVER_NAMESPACE::WListenerOwner>> m_seatOwners;
};
