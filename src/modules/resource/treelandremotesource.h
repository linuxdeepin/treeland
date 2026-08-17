// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "rep_treelandwindowtree_source.h"

#include <QPointF>
#include <QTimer>

class SurfaceWrapper;
class SurfaceContainer;
class WorkspaceModel;

class TreelandRemoteSource : public WindowTreeRemoteSource
{
    Q_OBJECT

public:
    explicit TreelandRemoteSource(QObject *parent = nullptr);
    ~TreelandRemoteSource() override;

    QPointF cursorPosition() const override;
    TreelandInfo getTreelandInfo() override;

    // ---- inspection ----
    QList<WindowInfo> getWindows() override;
    QList<ClientInfo> getClients() override;

    // ---- window control ----
    bool activateWindow(qint64 id) override;
    bool closeWindow(qint64 id) override;
    bool minimizeWindow(qint64 id) override;
    bool toggleMaximized(qint64 id) override;
    bool toggleFullscreen(qint64 id) override;
    bool moveWindow(qint64 id, int x, int y) override;
    bool resizeWindow(qint64 id, int w, int h) override;
    bool setWindowWorkspace(qint64 id, int workspaceId) override;

    // ---- input / event injection ----
    bool moveCursor(QPointF pos) override;
    bool sendPointerButton(int button, bool pressed) override;
    bool sendKey(int keycode, bool pressed) override;

    // ---- image capture ----
    // Returns raw PNG bytes; the treeland-debug client is responsible for
    // writing them to disk. The compositor never touches the filesystem.
    QByteArray captureOutput(QString outputName) override;
    QByteArray captureWindow(qint64 id) override;
    QByteArray captureScreen() override;

    // ---- real-time monitoring ----
    QList<DebugEvent> getEvents(quint64 afterSeq) override;
    qint64 focusedWindowId() override;
    qint64 windowUnderCursor() override;

protected:
    // Observes input events delivered to the render window. Installed lazily
    // only while a treeland-debug client is actively polling getEvents(), so
    // there is zero per-event overhead when nobody is monitoring.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void collectSurfaceInfos(QList<WindowInfo> &infos,
                             SurfaceWrapper *surface,
                             int layer,
                             const QString &containerName,
                             int z) const;
    void collectWorkspaceModelWindows(QList<WindowInfo> &infos,
                                      WorkspaceModel *workspaceModel,
                                      int layer,
                                      const QString &containerName) const;
    void collectCurrentWorkspaceModelWindows(QList<WindowInfo> &infos,
                                             WorkspaceModel *workspaceModel,
                                             int layer,
                                             const QString &containerName) const;
    WindowInfo buildWindowInfo(SurfaceWrapper *surface,
                               int layer,
                               const QString &containerName,
                               int z) const;
    LayerInfo buildLayerInfo(SurfaceContainer *container) const;

    // Collects every toplevel SurfaceWrapper (no parent surface) reachable from
    // the root container, in a stable display order. Used by the flat window
    // list, the client grouping and the id -> surface lookup.
    void collectAllToplevelSurfaces(QList<SurfaceWrapper *> &out) const;

    // Resolves a surface id (the wl_surface wl_resource id) to a live
    // SurfaceWrapper, or nullptr if no current surface matches.
    SurfaceWrapper *findSurfaceById(qint64 id) const;

    // Starts/stops capturing input events into m_events. Capturing is gated on
    // a live treeland-debug poller: it is armed on every getEvents() call and
    // torn down after a short idle interval, guaranteeing no overhead when no
    // client is connected.
    void startEventCapture();
    void stopEventCapture();
    void appendEvent(int type, qint64 target, const QString &detail);

    // Event ring buffer for real-time monitoring; populated only while a client
    // is actively polling getEvents().
    QList<DebugEvent> m_events;
    quint64 m_nextEventSeq = 1;
    static constexpr int MAX_EVENTS = 2000;
    bool m_eventCaptureActive = false;
    QTimer *m_eventIdleTimer = nullptr;
};
