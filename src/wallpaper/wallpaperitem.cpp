// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperitem.h"

#include "core/qmlengine.h"
#include "seat/helper.h"
#include "modules/personalization/personalizationmanager.h"
#include "workspace/workspacemodel.h"
#include "wallpapershellinterfacev1.h"
#include "wallpaper/wallpapermanager.h"
#include "workspace.h"
#include "shellhandler.h"

#include <woutputitem.h>
#include <woutput.h>

#include <QTimer>
#include <QLoggingCategory>

WAYLIB_SERVER_USE_NAMESPACE

WallpaperItem::WallpaperItem(QQuickItem *parent)
    : WSurfaceItemContent(parent)
{
    m_model = Helper::instance()->qmlEngine()->singletonInstance<UserModel *>("Treeland",
                                                                              "UserModel");
    connect(m_model,
            &UserModel::currentUserNameChanged,
            this,
            &WallpaperItem::handleCurrentuserChanged);
    connect(Helper::instance()->shellHandler()->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            this,
            &WallpaperItem::handleWallpaperSurfaceAdded);
    connect(Helper::instance(),
            &Helper::updateWallpaper,
            this,
            &WallpaperItem::updateSurface);
    connect(Helper::instance()->workspace(),
            &Workspace::workspaceAdded,
            this,
            &WallpaperItem::handleWorkspaceAdded);
}

WallpaperItem::~WallpaperItem() = default;

WorkspaceModel *WallpaperItem::workspace()
{
    return m_workspace;
}

void WallpaperItem::setWorkspace(WorkspaceModel *workspace)
{
    if (m_workspace == workspace) {
        return;
    }

    m_workspace = workspace;
    Q_EMIT workspaceChanged();
    if (wallpaperRole() == Lockscreen)
        return;

    updateSurface();
}

WOutput *WallpaperItem::output()
{
    return m_output;
}

void WallpaperItem::setOutput(WOutput *output)
{
    if (m_output == output) {
        return;
    }

    m_output = output;
    Q_EMIT outputChanged();
    updateSurface();
}

void WallpaperItem::setWallpaperRole(WallpaperRole role)
{
    if (m_wallpaperRole == role) {
        return;
    }

    m_wallpaperRole = role;
    Q_EMIT wallpaperRoleChanged();
    updateSurface();
}

void WallpaperItem::setWallpaperState(WallpaperState state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    Q_EMIT wallpaperStateChanged();
}

WallpaperItem::WallpaperState WallpaperItem::wallpaperState()
{
    return m_state;
}

QString WallpaperItem::source() const
{
    return m_source;
}

bool WallpaperItem::play() const
{
    return m_play;
}

void WallpaperItem::setPlay(bool value)
{
    if (m_play == value) {
        return;
    }

    TreelandWallpaperSurfaceInterfaceV1 *interface =
        TreelandWallpaperSurfaceInterfaceV1::get(source());
    if (!interface) {
        return;
    }
    interface->setPlay(value);
    m_play = value;
    Q_EMIT playChanged();
}

void WallpaperItem::slowDown()
{
    TreelandWallpaperSurfaceInterfaceV1 *interface =
        TreelandWallpaperSurfaceInterfaceV1::get(source());
    if (!interface) {
        return;
    }

    interface->slowDown();
}

void WallpaperItem::handleCurrentuserChanged()
{
    updateSurface();
}

WallpaperItem::WallpaperRole WallpaperItem::wallpaperRole()
{
    return m_wallpaperRole;
}

void WallpaperItem::updateSurface()
{
    if (!output()) {
        return;
    }

    if (wallpaperRole() == Desktop && !workspace()) {
        return;
    }

    WallpaperOutputConfig config =
        Helper::instance()->m_wallpaperManager->getOutputConfig(output()->nativeHandle());
    if (wallpaperRole() == Lockscreen) {
        if (config.lockscreenWallpaper != m_source) {
                TreelandWallpaperSurfaceInterfaceV1 *interface =
                    TreelandWallpaperSurfaceInterfaceV1::get(config.lockscreenWallpaper);
                if (!interface) {
                    return;
                }
                m_source = config.lockscreenWallpaper;
                setSurface(interface->wSurface());
                interface->wSurface()->enterOutput(output());
                Q_EMIT sourceChanged();
        }
        return;
    }

    if (wallpaperRole() == Desktop) {
        if (!workspace()) {
            return;
        }
        for (WallpaperWorkspaceConfig workspaceConfig : std::as_const(config.workspaces)) {
            if (workspaceConfig.workspaceId == workspace()->id() &&
                workspaceConfig.desktopWallpaper != m_source) {
                TreelandWallpaperSurfaceInterfaceV1 *interface =
                    TreelandWallpaperSurfaceInterfaceV1::get(workspaceConfig.desktopWallpaper);
                if (!interface) {
                    return;
                }
                m_source = workspaceConfig.desktopWallpaper;
                setSurface(interface->wSurface());
                interface->wSurface()->enterOutput(output());
                Q_EMIT sourceChanged();
                break;
            }
        }
        return;
    }
}

void WallpaperItem::handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface)
{
    if (wallpaperRole() != Lockscreen &&
        Helper::instance()->m_wallpaperManager->getWallpaperType(interface->source()) == TreelandWallpaperInterfaceV1::Video) {
        QTimer::singleShot(1000,
                           this,
                           [this]{
                               updateSurface();
                               setPlay(false);
                           });
    } else {
        updateSurface();
    }
}

void WallpaperItem::handleWorkspaceAdded()
{
    if (wallpaperRole() == Lockscreen) {
        return;
    }

    if (!output() || !workspace()) {
        return;
    }

    Helper::instance()->m_wallpaperManager->syncAddWorkspace();
    updateSurface();
}
