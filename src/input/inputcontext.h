// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <Qt>
#include <QPointF>

class SeatSurfaceManager;
class SurfaceWrapper;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

// Fixed capabilities of one desktop. Input workers cannot look up another
// desktop or switch the owner of these capabilities.
class InputContext
{
public:
    virtual ~InputContext() = default;
    virtual bool available() const = 0;
    virtual bool locked() const = 0;
    virtual bool normal() const = 0;
    virtual bool switchVirtualTerminal(int number) = 0;
    virtual bool toggleDebugView() = 0;
    virtual WAYLIB_SERVER_NAMESPACE::WSeat *primarySeat() const = 0;
    virtual SeatSurfaceManager *seatContainer(WAYLIB_SERVER_NAMESPACE::WSeat *seat) const = 0;
    virtual SurfaceWrapper *surface(WAYLIB_SERVER_NAMESPACE::WSurface *surface) const = 0;
    virtual void activateSurface(WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                                 WAYLIB_SERVER_NAMESPACE::WSeat *seat, Qt::FocusReason reason) = 0;
    virtual void moveResize(WAYLIB_SERVER_NAMESPACE::WSeat *seat, const QPointF &delta) = 0;
    virtual void endMoveResize(WAYLIB_SERVER_NAMESPACE::WSeat *seat) = 0;
    virtual void cancelMoveResize(WAYLIB_SERVER_NAMESPACE::WSeat *seat) = 0;
    virtual void detectEdgeTiling(WAYLIB_SERVER_NAMESPACE::WSeat *seat) = 0;
};
