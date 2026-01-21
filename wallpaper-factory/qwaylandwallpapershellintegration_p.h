// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-wallpaper-shell-unstable-v1.h"

#include <QQuickView>

#include <QtWaylandClient/private/qwaylandshellintegration_p.h>

class QWaylandWallpaperShellIntegration : public QtWaylandClient::QWaylandShellIntegrationTemplate<QWaylandWallpaperShellIntegration>,
                                            public QtWayland::treeland_wallpaper_shell_v1
{
public:
    QWaylandWallpaperShellIntegration();
    ~QWaylandWallpaperShellIntegration() override;

    QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;
};
