// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "pointerconstraintsmanager.h"
#include "helper.h"

#include <wcursor.h>
#include <wglobal.h>
#include <wseat.h>
#include <wserver.h>

#include <wlr_all.h>

PointerConstraintsManager::PointerConstraintsManager(WAYLIB_SERVER_NAMESPACE::WPointerConstraintsV1 *constraints,
                                                     Helper *helper, QObject *parent)
    : QObject(parent)
    , m_interface(constraints)
    , m_helper(helper)
{
    if (m_interface) {
        // The wrapper's own new_constraint forwarder is registered on the
        // WPointerConstraintsV1 interface's WObject listener list (in its
        // create()) and torn down with WServer::stop(); here we only bridge
        // it to our Qt slot.
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

    // Per-constraint owner: set_region + destroy. Erasing this owner (in
    // onConstraintDestroyed) tears down both listeners before wlroots
    // asserts the constraint's signal lists are empty and frees it.
    m_constraintOwners[constraint] = std::make_unique<WAYLIB_SERVER_NAMESPACE::WListenerOwner>();
    auto *constraintOwner = m_constraintOwners[constraint].get();
    constraintOwner->listeners()->add(&constraint->events.set_region, this,
                            [this, constraint]() {
                                // wlroots updates the effective region
                                // automatically; re-evaluate activation.
                                if (!m_active.contains(constraint))
                                    activateConstraint(constraint);
                            });
    constraintOwner->listeners()->add(&constraint->events.destroy, this,
                            [this, constraint]() {
                                onConstraintDestroyed(constraint);
                            });

    ensureSeatTracked(constraint->seat);

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
    // grab (popup grab, implicit move/resize grab, etc.). pointer_state.grab
    // is always non-null (it defaults to default_grab), so use
    // wlr_seat_pointer_has_grab which compares the grab interface against
    // the default.
    if (constraint->seat->drag)
        return false;
    if (wlr_seat_pointer_has_grab(constraint->seat))
        return false;

    // treeland move/resize is Qt-level and does not raise a seat grab, so it
    // must be excluded explicitly via the Helper accessor.
    auto *seat = WAYLIB_SERVER_NAMESPACE::WSeat::fromHandle(constraint->seat);
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

    auto *seat = WAYLIB_SERVER_NAMESPACE::WSeat::fromHandle(constraint->seat);
    auto *cursor = seat ? seat->cursor() : nullptr;
    if (!cursor)
        return;

    m_cursors[constraint] = cursor;
    cursor->setActivePointerConstraint(constraint);

    wlr_pointer_constraint_v1_send_activated(constraint);

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
    // not touch the constraint object after this call (the destroy listener
    // above will fire and clean up the owner).
    wlr_pointer_constraint_v1_send_deactivated(constraint);
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

    // Erase the per-constraint owner last: ~WListenerOwner -> teardown
    // disconnects the set_region and destroy listeners *during* this destroy
    // callback (safe: WScopedListener supports self-disconnect), so that
    // wlroots' subsequent assert(wl_list_empty(&events.set_region/destroy))
    // holds and free(constraint) does not leave dangling wl_listener nodes.
    m_constraintOwners.erase(constraint);
}

void PointerConstraintsManager::ensureSeatTracked(wlr_seat *seat)
{
    if (!seat || m_seatOwners.count(seat))
        return;

    m_seatOwners[seat] = std::make_unique<WAYLIB_SERVER_NAMESPACE::WListenerOwner>();
    auto *seatOwner = m_seatOwners[seat].get();
    // A grab started or ended on this seat: deactivate its constraints, then
    // (only after the grab ended) re-evaluate activation.
    seatOwner->listeners()->add(&seat->events.pointer_grab_begin, this,
                            [this, seat]() { onSeatGrabChanged(seat); });
    seatOwner->listeners()->add(&seat->events.pointer_grab_end, this,
                            [this, seat]() { onSeatGrabChanged(seat); });
    // Re-evaluate when pointer focus changes: a constraint created while the
    // pointer was elsewhere must activate once focus enters its surface, and
    // an active constraint must deactivate when focus leaves it.
    seatOwner->listeners()->add(&seat->pointer_state.events.focus_change, this,
                            [this, seat]() { onPointerFocusChanged(seat); });
    // Seat destruction: wlroots destroys every constraint on the seat first
    // (via each constraint's own seat_destroy listener, which runs our
    // onConstraintDestroyed and erases its owner), so here we only drop the
    // seat's own owner. Erasing it -> teardown disconnects grab/focus/destroy
    // listeners before wlr_seat_destroy asserts those lists are empty.
    seatOwner->listeners()->add(&seat->events.destroy, this,
                            [this, seat]() {
                                m_seatOwners.erase(seat);
                            });
}

void PointerConstraintsManager::onSeatGrabChanged(wlr_seat *seat)
{
    // Always deactivate first so a grab (drag/popup) takes over the pointer.
    const auto active = m_active;
    for (auto *constraint : active) {
        if (constraint->seat == seat)
            deactivateConstraint(constraint);
    }

    // Only re-evaluate (potentially re-activate) after the grab has ended.
    // During an active grab canActivate's grab check would reject anyway, and
    // re-evaluating on grab_begin would immediately undo the deactivation
    // above.
    if (wlr_seat_pointer_has_grab(seat))
        return;

    const auto tracked = m_constraints;
    for (auto *constraint : tracked) {
        if (constraint->seat == seat)
            activateConstraint(constraint);
    }
}

void PointerConstraintsManager::onPointerFocusChanged(wlr_seat *seat)
{
    // Deactivate constraints whose surface lost focus and activate newly
    // eligible ones (e.g. a constraint created before the pointer entered).
    const auto tracked = m_constraints;
    for (auto *constraint : tracked) {
        if (constraint->seat != seat)
            continue;

        const bool focused = seat->pointer_state.focused_surface == constraint->surface;
        if (m_active.contains(constraint)) {
            if (!focused)
                deactivateConstraint(constraint);
        } else if (focused) {
            activateConstraint(constraint);
        }
    }
}

void PointerConstraintsManager::deactivateAll()
{
    const auto active = m_active;
    for (auto *constraint : active)
        deactivateConstraint(constraint);
}
