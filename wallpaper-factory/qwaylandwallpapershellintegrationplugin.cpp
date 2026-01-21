// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwaylandwallpapershellintegration_p.h"
#include <QtWaylandClient/private/qwaylandshellintegrationplugin_p.h>

class QWaylandWallpaperIntegrationPlugin : public QtWaylandClient::QWaylandShellIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QWaylandShellIntegrationFactoryInterface_iid FILE "wallpaper-shell.json")

public:
    QWaylandWallpaperIntegrationPlugin()
    {
    }

    QtWaylandClient::QWaylandShellIntegration *create([[maybe_unused]] const QString &key,
                                                      [[maybe_unused]] const QStringList &paramList) override
    {
        return new QWaylandWallpaperShellIntegration();
    }
};

// Q_IMPORT_PLUGIN(QWaylandWallpaperIntegrationPlugin);

#include "qwaylandwallpapershellintegrationplugin.moc"
