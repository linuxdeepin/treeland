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

    QPointF m_cursorPosition;
    bool m_cursorTracking = false;
};
