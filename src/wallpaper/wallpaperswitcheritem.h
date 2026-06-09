// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include <QQuickItem>

Q_MOC_INCLUDE("workspace/workspace.h")

class WallpaperSlot;
class TreelandWallpaperSurfaceInterfaceV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class WorkspaceModel;

class WallpaperSwitcherItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutput *output READ output WRITE setOutput NOTIFY outputChanged FINAL)
    Q_PROPERTY(WorkspaceModel *workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged FINAL)
    Q_PROPERTY(bool play READ play WRITE setPlay NOTIFY playChanged FINAL)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged FINAL)
    Q_PROPERTY(int opacityDuration READ opacityDuration WRITE setOpacityDuration NOTIFY opacityDurationChanged FINAL)

    QML_NAMED_ELEMENT(WallpaperSwitcher)
    QML_ADDED_IN_VERSION(1, 0)

public:
    explicit WallpaperSwitcherItem(QQuickItem *parent = nullptr);
    ~WallpaperSwitcherItem() override;

    WOutput *output() const;
    void setOutput(WOutput *output);

    WorkspaceModel *workspace() const;
    void setWorkspace(WorkspaceModel *workspace);

    bool play() const;
    void setPlay(bool value);

    QString source() const;

    int opacityDuration() const;
    void setOpacityDuration(int duration);

    Q_INVOKABLE void slowDown();

Q_SIGNALS:
    void outputChanged();
    void workspaceChanged();
    void playChanged();
    void sourceChanged();
    void opacityDurationChanged();

private:
    void handleWallpaperUpdate();
    void handleWorkspaceAdded();
    void switchToNewSlot();
    void onAnimationFinished();
    void startFadeIn(WallpaperSlot *slot);

    QPointer<WorkspaceModel> m_workspace;
    QPointer<WOutput> m_output;
    bool m_play = true;
    QString m_source;
    int m_opacityDuration = 500;

    WallpaperSlot *m_currentSlot = nullptr;
    WallpaperSlot *m_oldSlot = nullptr;
};
