// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwaylandwallpapersurface_p.h"
#include "qwaylandwallpapershellintegration_p.h"

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandsurface_p.h>

QWaylandWallpaperSurface::QWaylandWallpaperSurface(QWaylandWallpaperShellIntegration *shell,
                                QtWaylandClient::QWaylandWindow *window)
    : QtWaylandClient::QWaylandShellSurface(window)
    , QtWayland::treeland_wallpaper_surface_v1()
    , m_shell(shell)
    , m_interface(WallpaperWindow::get(window->window()))
    , m_window(window)
{
    init(shell->get_treeland_wallpaper_surface(window->waylandSurface()->object(), m_interface->source()));
}

QWaylandWallpaperSurface::~QWaylandWallpaperSurface()
{
    destroy();
}

void QWaylandWallpaperSurface::treeland_wallpaper_surface_v1_position([[maybe_unused]] wl_fixed_t position)
{

}

void QWaylandWallpaperSurface::treeland_wallpaper_surface_v1_pause()
{

}

void QWaylandWallpaperSurface::treeland_wallpaper_surface_v1_play()
{

}
