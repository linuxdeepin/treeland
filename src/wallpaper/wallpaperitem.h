// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wsurfaceitem.h"
#include "greeter/usermodel.h"

Q_MOC_INCLUDE("workspace/workspace.h")

class TreelandWallpaperSurfaceInterfaceV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class WorkspaceModel;

class WallpaperItem : public WSurfaceItemContent
{
    Q_OBJECT
    Q_PROPERTY(WorkspaceModel* workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutput* output READ output WRITE setOutput NOTIFY outputChanged FINAL)
    Q_PROPERTY(WallpaperRole wallpaperRole READ wallpaperRole WRITE setWallpaperRole NOTIFY wallpaperRoleChanged FINAL)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged FINAL)
    Q_PROPERTY(WallpaperState wallpaperState READ wallpaperState WRITE setWallpaperState NOTIFY wallpaperStateChanged FINAL)
    Q_PROPERTY(bool play READ play WRITE setPlay NOTIFY playChanged FINAL)

    QML_NAMED_ELEMENT(Wallpaper)
    QML_ADDED_IN_VERSION(1, 0)

public:
    enum WallpaperRole {
        Desktop    = 0x1,
        Lockscreen = 0x2
    };
    Q_ENUM(WallpaperRole)

    enum WallpaperState
    {
        Normal,
        Scale,
        ScaleWithoutAnimation,
    };
    Q_ENUM(WallpaperState)

    WallpaperItem(QQuickItem *parent = nullptr);
    ~WallpaperItem();

    WorkspaceModel *workspace();
    void setWorkspace(WorkspaceModel *workspace);

    WOutput *output();
    void setOutput(WOutput *output);

    enum WallpaperRole wallpaperRole();
    void setWallpaperRole(enum WallpaperRole role);

    enum WallpaperState wallpaperState();
    void setWallpaperState(enum WallpaperState state);

    QString source() const;

    bool play() const;
    void setPlay(bool value);

    Q_INVOKABLE void slowDown();

Q_SIGNALS:
    void outputChanged();
    void workspaceChanged();
    void wallpaperRoleChanged();
    void sourceChanged();
    void wallpaperStateChanged();
    void playChanged();

private Q_SLOTS:
    void handleCurrentuserChanged();
    void updateSurface();
    void handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface);
    void handleWorkspaceAdded();

private:
    int m_userId = -1;
    QPointer<WorkspaceModel> m_workspace = nullptr;
    QPointer<WOutput> m_output = nullptr;
    enum WallpaperRole m_wallpaperRole = Desktop;
    enum WallpaperState m_state = Normal;
    QString m_source;
    UserModel *m_model;
    bool m_play = false;
};
