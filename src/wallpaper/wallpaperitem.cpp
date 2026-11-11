// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperitem.h"

#include "core/qmlengine.h"
#include "core/waylandserver.h"
#include "workspace/workspacemodel.h"
#include "wallpapershellinterfacev1.h"
#include "wallpaper/wallpapermanager.h"
#include "workspace.h"
#include "shellhandler.h"
#include "greeterproxy.h"

#include <woutputitem.h>
#include <woutput.h>

#include <QTimer>
#include <QLoggingCategory>

WAYLIB_SERVER_USE_NAMESPACE

WallpaperItem::WallpaperItem(QQuickItem *parent)
    : WallpaperItem(parent, true)
{
}

WallpaperItem::WallpaperItem(QQuickItem *parent, bool autoUpdate)
    : WSurfaceItemContent(parent)
    , m_autoUpdate(autoUpdate)
{
    if (autoUpdate) {
        connect(WaylandServer::instance(),
                &WaylandServer::activeSessionChanged,
                this,
                &WallpaperItem::handleCurrentuserChanged);
        connect(WaylandServer::instance()->shellHandler()->wallpaperShell(),
                &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
                this,
                &WallpaperItem::handleWallpaperSurfaceAdded);
        connect(WaylandServer::instance(),
                &WaylandServer::wallpaperChanged,
                this,
                &WallpaperItem::updateSurface);
        connect(WaylandServer::instance(),
                &WaylandServer::workspaceAdded,
                this,
                &WallpaperItem::handleWorkspaceAdded);
    }
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

    if (m_workspace && m_workspace->workspaceManager()->wallpaperManager())
        disconnect(m_workspace->workspaceManager()->wallpaperManager(),
                   &WallpaperManager::updateWallpaper, this, &WallpaperItem::updateSurface);
    m_workspace = workspace;
    if (m_autoUpdate && m_workspace && m_workspace->workspaceManager()->wallpaperManager())
        connect(m_workspace->workspaceManager()->wallpaperManager(),
                &WallpaperManager::updateWallpaper, this, &WallpaperItem::updateSurface);
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

    auto *manager = workspace() && wallpaperRole() == Desktop
        ? workspace()->workspaceManager()->wallpaperManager()
        : WaylandServer::instance()->wallpaperManager();
    if (!manager)
        return;
    const auto config = manager->getOutputConfig(output()->handle());
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
                update();
        }
        return;
    }

    if (wallpaperRole() == Desktop) {
        const QString source = manager->desktopWallpaper(output(), workspace());
        if (source.isEmpty())
            return;
        auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(source);
        if (!interface || !interface->wallpaperReady())
            return;
        if (source == m_source && surface() == interface->wSurface())
            return;
        m_source = source;
        setSurface(interface->wSurface());
        interface->wSurface()->enterOutput(output());
        update();
        if (!WaylandServer::instance()->m_greeterProxy->isLocked())
            setPlay(false);
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

    updateSurface();
}

void WallpaperItem::handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface)
{
    if (interface->wallpaperReady()) {
        updateSurface();
    } else {
        connect(interface,
                &TreelandWallpaperSurfaceInterfaceV1::ready,
                this,
                &WallpaperItem::updateSurface,
                Qt::SingleShotConnection);
    }
}
