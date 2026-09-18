// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "greeter/usermodel.h"
#include "wglobal.h"
#include "wsurfaceitem.h"

#include <QColor>
#include <QFutureWatcher>

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
    Q_PROPERTY(QString source READ source FINAL)
    Q_PROPERTY(WallpaperState wallpaperState READ wallpaperState WRITE setWallpaperState NOTIFY wallpaperStateChanged FINAL)
    Q_PROPERTY(WallpaperType wallpaperType READ wallpaperType NOTIFY wallpaperTypeChanged FINAL)
    Q_PROPERTY(QColor wallpaperColor READ wallpaperColor NOTIFY wallpaperColorChanged FINAL)
    Q_PROPERTY(bool play READ play WRITE setPlay NOTIFY playChanged FINAL)

    QML_NAMED_ELEMENT(Wallpaper)
    QML_ADDED_IN_VERSION(1, 0)

public:
    enum WallpaperRole {
        Desktop    = 0x1,
        Lockscreen = 0x2
    };
    Q_ENUM(WallpaperRole)

    enum WallpaperType
    {
        Image = 0,
        Video = 1,
    };
    Q_ENUM(WallpaperType)

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
    enum WallpaperType wallpaperType();
    QColor wallpaperColor() const;

    bool play() const;
    void setPlay(bool value);
    void setWallpaperType(enum WallpaperType type);

    Q_INVOKABLE void slowDown();
    void updateSurface();
    void refreshWallpaperColor();
    void setWallpaperColor(const QColor &color);

Q_SIGNALS:
    void outputChanged();
    void workspaceChanged();
    void wallpaperRoleChanged();
    void wallpaperStateChanged();
    void playChanged();

    void wallpaperTypeChanged();
    void wallpaperColorChanged();
private Q_SLOTS:
    void handleCurrentuserChanged();
    void handleWorkspaceAdded();
    void handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface);

protected:
    WallpaperItem(QQuickItem *parent, bool autoUpdate);

private:
    int m_userId = -1;
    QPointer<WorkspaceModel> m_workspace = nullptr;
    QPointer<WOutput> m_output = nullptr;
    enum WallpaperRole m_wallpaperRole = Desktop;
    enum WallpaperType m_wallpaperType = Image;
    enum WallpaperState m_state = Normal;
    QColor m_wallpaperColor = Qt::transparent;
    QString m_colorSource;
    QFutureWatcher<QColor> m_colorWatcher;
    QString m_source;
    UserModel *m_model;
    bool m_play = true;
};
