// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qmlengine.h"
#include "wglobal.h"

#include <private/qquickimage_p.h>

Q_MOC_INCLUDE("workspace.h")

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class WorkspaceModel;

class WallpaperImage : public QQuickImage
{
    Q_OBJECT
    Q_PROPERTY(int userId READ userId WRITE setUserId NOTIFY userIdChanged FINAL)
    Q_PROPERTY(WorkspaceModel* workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutput* output READ output WRITE setOutput NOTIFY outputChanged FINAL)

    QML_NAMED_ELEMENT(Wallpaper)
    QML_ADDED_IN_VERSION(1, 0)

public:
    WallpaperImage(QQuickItem *parent = nullptr);
    ~WallpaperImage();

    int userId();
    void setUserId(const int id);

    WorkspaceModel *workspace();
    void setWorkspace(WorkspaceModel *workspace);

    WOutput *output();
    void setOutput(WOutput *output);

Q_SIGNALS:
    void userIdChanged();
    void outputChanged();
    void workspaceChanged();

protected:
    void updateSource();
    void updateWallpaperTexture(const QString &id, int size);

private:
    int m_userId = -1;
    QPointer<WorkspaceModel> m_workspace;
    QPointer<WOutput> m_output;
};
