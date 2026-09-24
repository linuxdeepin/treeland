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

class SnapTargetV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit SnapTargetV1(QObject *parent = nullptr);

    void addSnapMaskSurface(WSurface *surface);
    void removeSnapMaskSurface(WSurface *surface);

    WOutputRenderWindow *outputRenderWindow() const;
    void setOutputRenderWindow(WOutputRenderWindow *renderWindow);

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);

    void onNewResource(treeland_snap_target_v1 *snap);
    void onStartRequested(treeland_snap_target_v1 *snap, WSeat *seat, uint32_t events);
    void onStopRequested(treeland_snap_target_v1 *snap);
    void onResourceDestroyed(treeland_snap_target_v1 *snap);

    void onCursorMoved();
    void updateSnapTarget();
    void sendSnapTarget(const SnapTarget &target);
    void stopSnapping();

    wl_global *m_global{ nullptr };

    QList<QPointer<WSurface>> m_snapMaskSurfaces;
    QPointer<WOutputRenderWindow> m_renderWindow;
    QPointer<WSeat> m_seat;

    QPointer<treeland_snap_target_v1> m_activeResource;
    bool m_pidfdEnabled{ false };
    QMetaObject::Connection m_cursorConn;
    QList<SnapTarget> m_snapshot;
    SnapTarget m_lastTarget;
};
