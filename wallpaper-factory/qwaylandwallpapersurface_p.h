// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wallpaperwindow.h"
#include "qwayland-treeland-wallpaper-shell-unstable-v1.h"

#include <QtWaylandClient/private/qwaylandshellsurface_p.h>

class QWaylandWallpaperShellIntegration;

class QWaylandWallpaperSurface : public QtWaylandClient::QWaylandShellSurface,
                                public QtWayland::treeland_wallpaper_surface_v1
{
    Q_OBJECT
public:
    QWaylandWallpaperSurface(QWaylandWallpaperShellIntegration *shell,
                                QtWaylandClient::QWaylandWindow *window);
    ~QWaylandWallpaperSurface() override;

    bool isExposed() const override { return m_configured; }

private:
    void treeland_wallpaper_surface_v1_position(wl_fixed_t position) override;
    void treeland_wallpaper_surface_v1_pause() override;
    void treeland_wallpaper_surface_v1_play() override;
    void treeland_wallpaper_surface_v1_slow_down(uint32_t duration) override;

private:
    QWaylandWallpaperShellIntegration *m_shell;
    WallpaperWindow *m_interface;
    QtWaylandClient::QWaylandWindow *m_window;
    QSize m_pendingSize;
    QString m_activationToken;

    bool m_configured = true;
};
