// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "snaphandler.h"

#include <wcursor.h>
#include <woutputrenderwindow.h>
#include <wseat.h>
#include <wserver.h>

SnapTargetV1::SnapTargetV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
{
}

QByteArrayView SnapTargetV1::interfaceName() const
{
    return treeland_snap_target_v1_interface.name;
}

void SnapTargetV1::create(WServer *server)
{
    m_global = wl_global_create(server->handle(),
                                &treeland_snap_target_v1_interface,
                                1,
                                this,
                                SnapTargetV1::bind);
}

void SnapTargetV1::destroy([[maybe_unused]] WServer *server)
{
    this->disconnect();
    if (m_global) {
        wl_global_remove(m_global);
        m_global = nullptr;
    }
}

wl_global *SnapTargetV1::global() const
{
    return m_global;
}

void SnapTargetV1::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto *snapSession = static_cast<SnapTargetV1 *>(data);
    auto *snap = treeland_snap_target_v1_create_resource(client, version, id);
    if (!snap)
        return;
    snapSession->onNewResource(snap);
}

void SnapTargetV1::onNewResource(treeland_snap_target_v1 *snap)
{
    connect(snap, &treeland_snap_target_v1::startRequested, this, [this, snap](uint32_t events) {
        onStartRequested(snap, events);
    });
    connect(snap, &treeland_snap_target_v1::stopRequested, this, [this, snap]() {
        onStopRequested(snap);
    });
    connect(snap, &treeland_snap_target_v1::beforeDestroy, this, [this, snap]() {
        onResourceDestroyed(snap);
    });
}

void SnapTargetV1::onStartRequested(treeland_snap_target_v1 *snap, uint32_t events)
{
    if (!snap || !snap->resource)
        return;

    // Only one snap session may be active across all clients. start on an
    // already active snap object is ignored; start by another client fails
    // with the snap_busy reason.
    if (m_activeResource) {
        if (m_activeResource != snap)
            snap->sendFailed(TREELAND_SNAP_TARGET_V1_FAILURE_REASON_SNAP_BUSY);
        return;
    }

    m_activeResource = snap;
    m_pidfdEnabled = events & TREELAND_SNAP_TARGET_V1_EVENT_TYPE_PIDFD;

    m_snapshot = SnapDetector::collect(m_renderWindow, m_snapMaskSurface);

    auto seat = m_seat;
    if (seat && seat->cursor()) {
        m_cursorConn =
            connect(seat->cursor(), &WCursor::positionChanged, this, &SnapTargetV1::onCursorMoved);
    }

    onCursorMoved();
}

void SnapTargetV1::onStopRequested(treeland_snap_target_v1 *snap)
{
    if (m_activeResource != snap)
        return;
    stopSnapping();
}

void SnapTargetV1::onResourceDestroyed(treeland_snap_target_v1 *snap)
{
    if (m_activeResource == snap)
        stopSnapping();
}

void SnapTargetV1::stopSnapping()
{
    if (!m_activeResource)
        return;

    m_activeResource = nullptr;
    m_pidfdEnabled = false;

    if (m_cursorConn)
        disconnect(m_cursorConn);

    m_snapshot.clear();
    m_lastTarget = SnapTarget();
}

void SnapTargetV1::onCursorMoved()
{
    updateSnapTarget();
}

void SnapTargetV1::updateSnapTarget()
{
    if (!m_activeResource)
        return;

    auto seat = m_seat;
    if (!seat || !seat->cursor())
        return;

    auto cursorPos = seat->cursor()->position();
    auto target = SnapDetector::hitTest(cursorPos, m_snapshot);

    if (target == m_lastTarget)
        return;

    m_lastTarget = target;
    sendSnapTarget(target);
}

void SnapTargetV1::sendSnapTarget(const SnapTarget &target)
{
    if (!m_activeResource || !m_activeResource->resource)
        return;

    // The pidfd event is sent only when the client enabled it and the target
    // has a determinable owning process; it always precedes the snap_region
    // event that commits it.
    if (m_pidfdEnabled && !target.rect.isEmpty() && target.shellSurface) {
        int pidfd = target.shellSurface->pidFD();
        if (pidfd >= 0)
            m_activeResource->sendPidfd(pidfd);
    }

    const QRectF &region = target.rect;
    if (region.isValid() && !region.isEmpty()) {
        m_activeResource->sendSnapRegion(static_cast<int32_t>(region.x()),
                                         static_cast<int32_t>(region.y()),
                                         static_cast<uint32_t>(region.width()),
                                         static_cast<uint32_t>(region.height()));
    } else {
        m_activeResource->sendSnapRegion(0, 0, 0, 0);
    }
}

WSurface *SnapTargetV1::snapMaskSurface() const
{
    return m_snapMaskSurface;
}

void SnapTargetV1::setSnapMaskSurface(WSurface *surface)
{
    m_snapMaskSurface = surface;
}

WOutputRenderWindow *SnapTargetV1::outputRenderWindow() const
{
    return m_renderWindow;
}

void SnapTargetV1::setOutputRenderWindow(WOutputRenderWindow *renderWindow)
{
    m_renderWindow = renderWindow;
}

WSeat *SnapTargetV1::seat() const
{
    return m_seat;
}

void SnapTargetV1::setSeat(WSeat *seat)
{
    m_seat = seat;
}
