// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "pointerconstraintsmanager.h"
#include "helper.h"

#include <wcursor.h>

#include <qwpointerconstraintsv1.h>
#include <qwseat.h>

#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_seat.h>

PointerConstraintsManager::PointerConstraintsManager(WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *constraints,
                                                     Helper *helper, QObject *parent)
    : QObject(parent)
    , m_interface(constraints)
    , m_helper(helper)
{
    if (m_interface) {
        connect(m_interface, &WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1::newConstraint,
                this, &PointerConstraintsManager::onNewConstraint);
    }
}

PointerConstraintsManager::~PointerConstraintsManager() = default;

void PointerConstraintsManager::onNewConstraint(wlr_pointer_constraint_v1 *constraint)
{
    if (!constraint)
        return;

    m_constraints.insert(constraint);

    auto *qc = QW_NAMESPACE::qw_pointer_constraint_v1::from(constraint);

    connect(qc, &QW_NAMESPACE::qw_pointer_constraint_v1::notify_set_region, this, [this, constraint]() {
        // wlroots updates the effective region automatically; re-evaluate activation.
        if (!m_active.contains(constraint))
            activateConstraint(constraint);
    });

    connect(qc, &QW_NAMESPACE::qw_object_basic::before_destroy, this, [this, constraint]() {
        onConstraintDestroyed(constraint);
    });

    // Track grab state on the constraint's seat so we can deactivate during drag/popup.
    if (constraint->seat) {
        auto *qwSeat = QW_NAMESPACE::qw_seat::from(constraint->seat);
        if (qwSeat && !m_connectedSeats.contains(qwSeat)) {
            m_connectedSeats.insert(qwSeat);
            connect(qwSeat, &QW_NAMESPACE::qw_seat::notify_pointer_grab_begin, this, [this, seat = constraint->seat]() {
                onSeatGrabChanged(seat);
            });
            connect(qwSeat, &QW_NAMESPACE::qw_seat::notify_pointer_grab_end, this, [this, seat = constraint->seat]() {
                onSeatGrabChanged(seat);
            });
            connect(qwSeat, &QW_NAMESPACE::qw_object_basic::before_destroy, this, [this, qwSeat]() {
                m_connectedSeats.remove(qwSeat);
            });
        }
    }

    activateConstraint(constraint);
}

bool PointerConstraintsManager::canActivate(wlr_pointer_constraint_v1 *constraint) const
{
    if (!constraint || !constraint->seat || !constraint->surface)
        return false;

    // Never activate while the compositor is in a special mode.
    if (m_helper->currentMode() != Helper::CurrentMode::Normal)
        return false;

    // Don't activate during an active drag or any other (non-default) pointer
    // grab (popup grab, implicit move/resize grab, etc.). pointer_state.grab is
    // always non-null (it defaults to default_grab), so use wlr_seat_pointer_has_grab
    // which compares the grab interface against the default.
    if (constraint->seat->drag)
        return false;
    if (wlr_seat_pointer_has_grab(constraint->seat))
        return false;

    // treeland move/resize is Qt-level and does not raise a seat grab, so it
    // must be excluded explicitly via the Helper accessor.
    auto *seat = WAYLIB_SERVER_NAMESPACE::WSeat::fromHandle(QW_NAMESPACE::qw_seat::from(constraint->seat));
    if (seat && m_helper->isInMoveResize(seat))
        return false;

    // The constraint's surface must currently hold pointer focus.
    if (constraint->seat->pointer_state.focused_surface != constraint->surface)
        return false;

    return true;
}

void PointerConstraintsManager::activateConstraint(wlr_pointer_constraint_v1 *constraint)
{
    if (m_active.contains(constraint))
        return;

    if (!canActivate(constraint))
        return;

    auto *seat = WAYLIB_SERVER_NAMESPACE::WSeat::fromHandle(QW_NAMESPACE::qw_seat::from(constraint->seat));
    auto *cursor = seat ? seat->cursor() : nullptr;
    if (!cursor)
        return;

    m_cursors[constraint] = cursor;
    cursor->setActivePointerConstraint(constraint);

    auto *qc = QW_NAMESPACE::qw_pointer_constraint_v1::from(constraint);
    qc->send_activated();

    m_active.insert(constraint);
}

void PointerConstraintsManager::deactivateConstraint(wlr_pointer_constraint_v1 *constraint)
{
    if (!m_active.remove(constraint))
        return;

    auto it = m_cursors.find(constraint);
    if (it != m_cursors.end() && it.value())
        it.value()->setActivePointerConstraint(nullptr);

    // send_deactivated may destroy the constraint for oneshot lifetime, so do
    // not touch the constraint object after this call.
    auto *qc = QW_NAMESPACE::qw_pointer_constraint_v1::from(constraint);
    qc->send_deactivated();
}

void PointerConstraintsManager::onConstraintDestroyed(wlr_pointer_constraint_v1 *constraint)
{
    m_constraints.remove(constraint);
    bool wasActive = m_active.remove(constraint);
    auto it = m_cursors.find(constraint);
    if (it != m_cursors.end()) {
        if (wasActive && it.value())
            it.value()->setActivePointerConstraint(nullptr);
        m_cursors.erase(it);
    }
}

void PointerConstraintsManager::onSeatGrabChanged(wlr_seat *seat)
{
    // A grab started or ended on this seat: always deactivate its constraints.
    const auto active = m_active;
    for (auto *constraint : active) {
        if (constraint->seat == seat)
            deactivateConstraint(constraint);
    }

    // Only re-evaluate (potentially re-activate) after the grab has ended.
    // During an active grab canActivate's grab check would reject anyway, and
    // re-evaluating on grab_begin would immediately undo the deactivation above.
    if (wlr_seat_pointer_has_grab(seat))
        return;

    const auto tracked = m_constraints;
    for (auto *constraint : tracked) {
        if (constraint->seat == seat)
            activateConstraint(constraint);
    }
}

void PointerConstraintsManager::deactivateAll()
{
    const auto active = m_active;
    for (auto *constraint : active)
        deactivateConstraint(constraint);
}
