// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "rep_treelandwindowtree_source.h"

#include <QPointF>

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
    QString captureOutput(QString outputName, QString filePath) override;
    QString captureWindow(qint64 id, QString filePath) override;
    QString captureScreen(QString filePath) override;

    // ---- real-time monitoring ----
    QList<DebugEvent> getEvents(quint64 afterSeq) override;
    qint64 focusedWindowId() override;
    qint64 windowUnderCursor() override;
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
    void updateCursor(const QPointF &newPosition);

    // Collects every toplevel SurfaceWrapper (no parent surface) reachable from
    // the root container, in a stable display order. Used by the flat window
    // list, the client grouping and the id -> surface lookup.
    void collectAllToplevelSurfaces(QList<SurfaceWrapper *> &out) const;

    // Resolves a stable surface id (the SurfaceWrapper pointer) to a live
    // SurfaceWrapper, or nullptr if no current surface matches.
    SurfaceWrapper *findSurfaceById(qint64 id) const;

    QPointF m_cursorPosition;
    bool m_cursorTracking = false;

    // Event ring buffer for real-time monitoring.
    QList<DebugEvent> m_events;
    quint64 m_nextEventSeq = 1;
    static constexpr int MAX_EVENTS = 2000;
};
