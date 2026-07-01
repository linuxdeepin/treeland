// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwaylandwallpapersurface_p.h"
#include "qwaylandwallpapershellintegration_p.h"

#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <private/qquickanimatedimage_p.h>

#define TREELAND_WALLPAPER_PRODUCE_V1_VERSION 1

QWaylandWallpaperShellIntegration::QWaylandWallpaperShellIntegration()
    : QWaylandShellIntegrationTemplate<QWaylandWallpaperShellIntegration>(TREELAND_WALLPAPER_PRODUCE_V1_VERSION)
{
}

QWaylandWallpaperShellIntegration::~QWaylandWallpaperShellIntegration()
{
    if (object()) {
        treeland_wallpaper_shell_v1_destroy(object());
    }
}

QtWaylandClient::QWaylandShellSurface *QWaylandWallpaperShellIntegration::createShellSurface(QtWaylandClient::QWaylandWindow *window)
{
    return new QWaylandWallpaperSurface(this, window);
}
