// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "snaphandler.h"

#include <wcursor.h>
#include <woutputrenderwindow.h>
#include <wseat.h>
#include <wserver.h>

CaptureSnapV1::CaptureSnapV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
{
}

QByteArrayView CaptureSnapV1::interfaceName() const
{
    return treeland_capture_snap_v1_interface.name;
}

void CaptureSnapV1::create(WServer *server)
{
    m_global = wl_global_create(server->handle(),
                                &treeland_capture_snap_v1_interface,
                                1,
                                this,
                                CaptureSnapV1::bind);
}

void CaptureSnapV1::destroy([[maybe_unused]] WServer *server)
{
    this->disconnect();
    if (m_global) {
        wl_global_remove(m_global);
        m_global = nullptr;
    }
}

wl_global *CaptureSnapV1::global() const
{
    return m_global;
}

void CaptureSnapV1::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto *snapSession = static_cast<CaptureSnapV1 *>(data);
    auto *snap = treeland_capture_snap_v1_create_resource(client, version, id);
    if (!snap)
        return;
    snapSession->onNewResource(snap);
}

void CaptureSnapV1::onNewResource(treeland_capture_snap_v1 *snap)
{
    connect(snap, &treeland_capture_snap_v1::startRequested, this, [this, snap]() {
        onStartRequested(snap);
    });
    connect(snap, &treeland_capture_snap_v1::stopRequested, this, [this, snap]() {
        onStopRequested(snap);
    });
    connect(snap, &treeland_capture_snap_v1::beforeDestroy, this, [this, snap]() {
        onResourceDestroyed(snap);
    });
}

void CaptureSnapV1::onStartRequested(treeland_capture_snap_v1 *snap)
{
    if (!snap || !snap->resource)
        return;

    // Only one snap session may be active across all clients. start on an
    // already active snap object is ignored; start by another client fails
    // with the snap_busy reason.
    if (m_activeResource) {
        if (m_activeResource != snap)
            snap->sendFailed(TREELAND_CAPTURE_SNAP_V1_FAILURE_REASON_SNAP_BUSY);
        return;
    }

    m_activeResource = snap;

    m_snapshot = SnapDetector::collect(m_renderWindow, m_captureMaskSurface);

    auto seat = m_seat;
    if (seat && seat->cursor()) {
        m_cursorConn =
            connect(seat->cursor(), &WCursor::positionChanged, this, &CaptureSnapV1::onCursorMoved);
    }

    onCursorMoved();
}

void CaptureSnapV1::onStopRequested(treeland_capture_snap_v1 *snap)
{
    if (m_activeResource != snap)
        return;
    stopSnapping();
}

void CaptureSnapV1::onResourceDestroyed(treeland_capture_snap_v1 *snap)
{
    if (m_activeResource == snap)
        stopSnapping();
}

void CaptureSnapV1::stopSnapping()
{
    if (!m_activeResource)
        return;

    m_activeResource = nullptr;

    if (m_cursorConn)
        disconnect(m_cursorConn);

    m_snapshot.clear();
    m_lastSnapRegion = QRectF();
}

void CaptureSnapV1::onCursorMoved()
{
    updateSnapRegion();
}

void CaptureSnapV1::updateSnapRegion()
{
    if (!m_activeResource)
        return;

    auto seat = m_seat;
    if (!seat || !seat->cursor())
        return;

    auto cursorPos = seat->cursor()->position();
    auto region = SnapDetector::hitTest(cursorPos, m_snapshot);

    if (region == m_lastSnapRegion)
        return;

    m_lastSnapRegion = region;
    sendSnapRegion(region);
}

void CaptureSnapV1::sendSnapRegion(const QRectF &region)
{
    if (!m_activeResource || !m_activeResource->resource)
        return;

    if (region.isValid() && !region.isEmpty()) {
        m_activeResource->sendSnapRegion(static_cast<int32_t>(region.x()),
                                         static_cast<int32_t>(region.y()),
                                         static_cast<uint32_t>(region.width()),
                                         static_cast<uint32_t>(region.height()));
    } else {
        m_activeResource->sendSnapRegion(0, 0, 0, 0);
    }
}

WSurface *CaptureSnapV1::captureMaskSurface() const
{
    return m_captureMaskSurface;
}

void CaptureSnapV1::setCaptureMaskSurface(WSurface *surface)
{
    m_captureMaskSurface = surface;
}

WOutputRenderWindow *CaptureSnapV1::outputRenderWindow() const
{
    return m_renderWindow;
}

void CaptureSnapV1::setOutputRenderWindow(WOutputRenderWindow *renderWindow)
{
    m_renderWindow = renderWindow;
}

WSeat *CaptureSnapV1::seat() const
{
    return m_seat;
}

void CaptureSnapV1::setSeat(WSeat *seat)
{
    m_seat = seat;
}
