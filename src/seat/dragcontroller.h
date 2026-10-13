// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wscoplistener.h>

#include <QObject>

#include <memory>
#include <vector>

struct wlr_drag;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SeatManager;

class DragController : public QObject
{
    Q_OBJECT
public:
    explicit DragController(SeatManager *seatManager, QObject *parent = nullptr);
    ~DragController() override;

    void initialize(bool notifyDdeShell);

private:
    void trackSeat(WSeat *seat);
    void handleRequestDrag(WSeat *seat, WSurface *surface);

private:
    struct SeatDragEntry {
        wlr_drag *drag = nullptr;
        std::unique_ptr<WListenerOwner> owner;
    };
    SeatManager *m_seatManager = nullptr;
    bool m_notifyDdeShell = false;
    std::vector<SeatDragEntry> seatDragEntries;
};
