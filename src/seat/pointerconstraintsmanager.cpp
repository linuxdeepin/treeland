// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "pointerconstraintsmanager.h"

#include "core/rootsurfacecontainer.h"
#include "helper.h"
#include "seatsmanager.h"
#include "surface/surfacewrapper.h"

#include <wcursor.h>
#include <wglobal.h>
#include <wlr_all.h>
#include <wseat.h>
#include <wserver.h>
#include <wsurface.h>

PointerConstraintsManager::PointerConstraintsManager(WPointerConstraintsV1 *constraints,
                                                     QObject *parent)
    : QObject(parent)
    , WObject()
    , m_interface(constraints)
{
    Q_ASSERT(m_interface);
    connect(m_interface,
            &WPointerConstraintsV1::newConstraint,
            this,
            &PointerConstraintsManager::onNewConstraint);

    // active-window change triggers constraint re-evaluation.
    connect(Helper::instance(), &Helper::activatedSurfaceChanged, this, [this]() {
        onActiveWindowChanged();
    });

    auto *helper = Helper::instance();
    auto *seatManager = helper->seatManager();
    for (auto *wseat : seatManager->seats())
        ensureSeatTracked(wseat->handle());
    connect(seatManager, &SeatsManager::seatAdded, this, [this](WSeat *wseat) {
        ensureSeatTracked(wseat->handle());
    });
}

PointerConstraintsManager::~PointerConstraintsManager()
{
    teardown();
}

void PointerConstraintsManager::onNewConstraint(wlr_pointer_constraint_v1 *constraint)
{
    if (!constraint)
        return;

    m_constraints.append(constraint);

    auto *constraintOwner = new WListenerOwner();
    constraint->data = constraintOwner;
    constraintOwner->listeners()->add(&constraint->events.set_region, this, [this, constraint]() {
        if (!m_active.contains(constraint)) {
            // Not yet active: try to activate now that
            // wlroots has updated the effective region.
            tryActivateConstraint(constraint);
            return;
        }

        // LOCKED: region changes do not affect a frozen cursor.
        if (constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
            return;

        // CONFINED: unconfine if the pointer leaves the new region.
        wlr_seat *wlrSeat = constraint->seat;
        if (!wlrSeat)
            return;
        const double sx = wlrSeat->pointer_state.sx;
        const double sy = wlrSeat->pointer_state.sy;
        if (pixman_region32_contains_point(&constraint->region, floor(sx), floor(sy), NULL))
            return;
        // Deferred: synchronous send_deactivated inside
        // wlr_signal_emit_mutable would assert-fail on the
        // still-iterated listener.
        QMetaObject::invokeMethod(
            this,
            [this, constraint]() {
                if (m_constraints.contains(constraint))
                    deactivateConstraint(constraint);
            },
            Qt::QueuedConnection);
    });
    constraintOwner->listeners()->add(&constraint->events.destroy, this, [this, constraint]() {
        onConstraintDestroyed(constraint);
    });

    tryActivateConstraint(constraint);
}

bool PointerConstraintsManager::canActivate(wlr_pointer_constraint_v1 *constraint) const
{
    if (!constraint || !constraint->seat || !constraint->surface)
        return false;

    // Never activate while the compositor is in a special mode.
    auto *helper = Helper::instance();
    if (helper->currentMode() != Helper::CurrentMode::Normal)
        return false;

    // Don't activate during any non-default pointer grab (popup, drag, etc.).
    // wlr_seat_pointer_has_grab covers drag too (drag pushes a grab).
    if (wlr_seat_pointer_has_grab(constraint->seat))
        return false;

    // treeland move/resize is Qt-level (no wlroots grab); exclude via Helper.
    auto *wseat = WSeat::fromHandle(constraint->seat);
    if (wseat && helper->rootContainer()->isInMoveResizeForSeat(wseat))
        return false;

    // Pointer focus: must match the constraint surface.
    if (constraint->seat->pointer_state.focused_surface != constraint->surface)
        return false;

    // Active window: only the foreground window may hold pointer constraints.
    auto *activated = helper->activatedSurface();
    if (!activated || !activated->surface()
        || activated->surface()->handle() != constraint->surface)
        return false;

    // Region containment: activate only if the pointer is already inside the
    // constraint region (lazy activation policy, matching KWin).
    wlr_seat *wlrSeat = constraint->seat;
    const double sx = wlrSeat->pointer_state.sx;
    const double sy = wlrSeat->pointer_state.sy;
    if (!pixman_region32_contains_point(&constraint->region, floor(sx), floor(sy), NULL))
        return false;

    return true;
}

void PointerConstraintsManager::tryActivateConstraint(wlr_pointer_constraint_v1 *constraint)
{
    if (m_active.contains(constraint))
        return;

    if (!canActivate(constraint))
        return;

    auto *seat = WSeat::fromHandle(constraint->seat);
    auto *cursor = seat ? seat->cursor() : nullptr;
    if (!cursor)
        return;

    cursor->setActivePointerConstraint(constraint);

    wlr_pointer_constraint_v1_send_activated(constraint);

    m_active.append(constraint);
}

void PointerConstraintsManager::releaseConstraintCursor(wlr_pointer_constraint_v1 *constraint)
{
    auto *seat = WSeat::fromHandle(constraint->seat);
    auto *cursor = seat ? seat->cursor() : nullptr;
    if (cursor) {
        cursor->warpToActiveConstraintHint();
        cursor->setActivePointerConstraint(nullptr);
    }
}

void PointerConstraintsManager::deactivateConstraint(wlr_pointer_constraint_v1 *constraint)
{
    if (!m_active.removeOne(constraint))
        return;

    releaseConstraintCursor(constraint);

    // send_deactivated may destroy the constraint for oneshot lifetime, so do
    // not touch the constraint object after this call (the destroy listener
    // above will fire and clean up the owner).
    wlr_pointer_constraint_v1_send_deactivated(constraint);
}

void PointerConstraintsManager::onConstraintDestroyed(wlr_pointer_constraint_v1 *constraint)
{
    m_constraints.removeOne(constraint);
    if (m_active.removeOne(constraint))
        releaseConstraintCursor(constraint);

    auto *owner = static_cast<WListenerOwner *>(constraint->data);
    constraint->data = nullptr;
    delete owner;
}

void PointerConstraintsManager::ensureSeatTracked(wlr_seat *seat)
{
    if (!seat)
        return;

    auto *wseat = WSeat::fromHandle(seat);
    if (!wseat)
        return;

    // the listener list is owned by this (PointerConstraintsManager)
    // on the wseat target. WSeat::teardown() in WServer::stop() auto-detaches all
    // cross-object listener groups from the target before destroy
    wseat->listeners(this)->add(&seat->events.pointer_grab_begin, this, [this, seat]() {
        onSeatGrabChanged(seat);
    });
    wseat->listeners(this)->add(&seat->events.pointer_grab_end, this, [this, seat]() {
        onSeatGrabChanged(seat);
    });
    // Activate on pointer enter, deactivate on pointer leave.
    wseat->listeners(this)->add(&seat->pointer_state.events.focus_change, this, [this, seat]() {
        onPointerFocusChanged(seat);
    });
}

void PointerConstraintsManager::onSeatGrabChanged(wlr_seat *seat)
{
    if (wlr_seat_pointer_has_grab(seat)) {
        // Grab began: deactivate all constraints for this seat.
        const auto active = m_active;
        for (auto *constraint : active) {
            if (constraint->seat == seat)
                deactivateConstraint(constraint);
        }
    } else {
        // Grab ended: re-evaluate all tracked constraints.
        const auto tracked = m_constraints;
        for (auto *constraint : tracked) {
            if (constraint->seat == seat)
                tryActivateConstraint(constraint);
        }
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
            tryActivateConstraint(constraint);
        }
    }
}

void PointerConstraintsManager::onActiveWindowChanged()
{
    auto *activated = Helper::instance()->activatedSurface();
    const auto activeWlrSurface =
        activated && activated->surface() ? activated->surface()->handle() : nullptr;

    const auto active = m_active;
    for (auto *c : active) {
        if (c->surface != activeWlrSurface)
            deactivateConstraint(c);
    }

    const auto tracked = m_constraints;
    for (auto *c : tracked) {
        if (c->surface == activeWlrSurface)
            tryActivateConstraint(c);
    }
}

void PointerConstraintsManager::deactivateAll()
{
    const auto active = m_active;
    for (auto *constraint : active)
        deactivateConstraint(constraint);
}
