// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessioninputcontext.h"
#include "session.h"
#include "sessionseatmanager.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "desktop/desktopenvironment.h"
#include "seat/seatmanager.h"
#include <wbackend.h>
#include <wsurface.h>
#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE

SessionInputContext::SessionInputContext(Session *owner, WaylandServer *runtime, QObject *parent)
    : QObject(parent), m_owner(owner), m_runtime(runtime)
{
}

bool SessionInputContext::available() const
{
    return m_runtime->isAvailable() && m_runtime->window() && m_runtime->backend()
        && (!m_owner || m_owner->isActive());
}

bool SessionInputContext::locked() const
{
    return m_runtime->currentMode() == WaylandServer::CurrentMode::LockScreen;
}

bool SessionInputContext::normal() const
{
    return m_runtime->currentMode() == WaylandServer::CurrentMode::Normal;
}

bool SessionInputContext::switchVirtualTerminal(int number)
{
    auto *backend = m_runtime->backend();
    if (!available() || !backend->isSessionActive())
        return false;
    wlr_session_change_vt(backend->session(), number);
    return true;
}

bool SessionInputContext::toggleDebugView()
{
    return available() && m_runtime->toggleDebugMenuBar();
}

bool SessionInputContext::ownsSeat(WSeat *seat) const
{
    if (!seat)
        return false;
    return m_owner ? m_owner->seatManager()->seats().contains(seat)
                   : m_runtime->seatManager()->seats().contains(seat);
}

WSeat *SessionInputContext::primarySeat() const
{
    return m_owner ? m_owner->seatManager()->fallbackSeat() : m_runtime->seat();
}

SeatSurfaceManager *SessionInputContext::seatContainer(WSeat *seat) const
{
    if (!ownsSeat(seat))
        return nullptr;
    auto *container = m_runtime->rootContainer()->getSeatContainer(seat);
    auto *moving = container ? container->moveResizeSurface() : nullptr;
    if (m_owner && moving && !m_owner->desktop()->surfaces()->hasObject(moving))
        return nullptr;
    return container;
}

SurfaceWrapper *SessionInputContext::surface(WSurface *native) const
{
    if (!native)
        return nullptr;
    auto *root = WSurface::fromHandle(wlr_surface_get_root_surface(native->handle()));
    if (!root)
        return nullptr;
    auto *wrapper = m_runtime->rootContainer()->getSurface(root);
    if (wrapper && !wrapper->renderingEnabled())
        return nullptr;
    if (m_owner && wrapper && !m_owner->desktop()->surfaces()->hasObject(wrapper))
        return nullptr;
    return wrapper;
}

void SessionInputContext::activateSurface(WSurface *native, WSeat *seat, Qt::FocusReason reason)
{
    if (!available() || !ownsSeat(seat))
        return;
    auto *wrapper = native ? surface(native) : nullptr;
    if (native && !wrapper)
        return;
    if (wrapper)
        m_runtime->updateSurfaceSeatInteraction(wrapper, seat);
    m_runtime->activateSurface(wrapper, reason, seat, true);
}

void SessionInputContext::moveResize(WSeat *seat, const QPointF &delta)
{
    if (available() && seatContainer(seat))
        m_runtime->rootContainer()->doMoveResizeForSeat(seat, delta);
}

void SessionInputContext::endMoveResize(WSeat *seat)
{
    if (available() && seatContainer(seat))
        m_runtime->rootContainer()->endMoveResizeForSeat(seat);
}

void SessionInputContext::cancelMoveResize(WSeat *seat)
{
    if (available() && seatContainer(seat))
        m_runtime->rootContainer()->cancelMoveResizeForSeat(seat);
}

void SessionInputContext::detectEdgeTiling(WSeat *seat)
{
    auto *container = seatContainer(seat);
    if (!available() || !container)
        return;
    auto *wrapper = container->moveResizeSurface();
    if (m_owner && wrapper && !m_owner->desktop()->surfaces()->hasObject(wrapper))
        return;
    m_runtime->rootContainer()->detectEdgeTilingForSeat(seat);
}
