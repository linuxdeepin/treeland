// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "input/inputcontext.h"
#include <QObject>

class Session;
class WaylandServer;

// Boundary adapter: identity and shared compositor services stay here, outside
// the event filter. A null owner is used only for the startup input path.
class SessionInputContext final : public QObject, public InputContext
{
public:
    SessionInputContext(Session *owner, WaylandServer *runtime, QObject *parent);
    bool available() const override;
    bool locked() const override;
    bool normal() const override;
    bool switchVirtualTerminal(int number) override;
    bool toggleDebugView() override;
    WAYLIB_SERVER_NAMESPACE::WSeat *primarySeat() const override;
    SeatSurfaceManager *seatContainer(WAYLIB_SERVER_NAMESPACE::WSeat *seat) const override;
    SurfaceWrapper *surface(WAYLIB_SERVER_NAMESPACE::WSurface *surface) const override;
    void activateSurface(WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                         WAYLIB_SERVER_NAMESPACE::WSeat *seat, Qt::FocusReason reason) override;
    void moveResize(WAYLIB_SERVER_NAMESPACE::WSeat *seat, const QPointF &delta) override;
    void endMoveResize(WAYLIB_SERVER_NAMESPACE::WSeat *seat) override;
    void cancelMoveResize(WAYLIB_SERVER_NAMESPACE::WSeat *seat) override;
    void detectEdgeTiling(WAYLIB_SERVER_NAMESPACE::WSeat *seat) override;
private:
    bool ownsSeat(WAYLIB_SERVER_NAMESPACE::WSeat *seat) const;
    Session *const m_owner;
    WaylandServer *const m_runtime;
};
