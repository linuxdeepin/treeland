// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "impl/snapv1impl.h"
#include "snapdetector.h"

#include <wglobal.h>
#include <wserver.h>
#include <wsurface.h>

#include <QPointer>
#include <QRectF>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputRenderWindow;
class WSeat;
class WCursor;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class CaptureSnapV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit CaptureSnapV1(QObject *parent = nullptr);

    WSurface *captureMaskSurface() const;
    void setCaptureMaskSurface(WSurface *surface);

    WOutputRenderWindow *outputRenderWindow() const;
    void setOutputRenderWindow(WOutputRenderWindow *renderWindow);

    WSeat *seat() const;
    void setSeat(WSeat *seat);

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);

    void onNewResource(treeland_capture_snap_v1 *snap);
    void onStartRequested(treeland_capture_snap_v1 *snap);
    void onStopRequested(treeland_capture_snap_v1 *snap);
    void onResourceDestroyed(treeland_capture_snap_v1 *snap);

    void onCursorMoved();
    void updateSnapRegion();
    void sendSnapRegion(const QRectF &region);
    void stopSnapping();

    wl_global *m_global{ nullptr };

    QPointer<WSurface> m_captureMaskSurface;
    QPointer<WOutputRenderWindow> m_renderWindow;
    QPointer<WSeat> m_seat;

    QPointer<treeland_capture_snap_v1> m_activeResource;
    QMetaObject::Connection m_cursorConn;
    QList<QRectF> m_snapshot;
    QRectF m_lastSnapRegion;
};
