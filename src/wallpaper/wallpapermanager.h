// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "output/output.h"
#include "wallpaper/wallpaperconfig.h"

#include <functional>
#include <QtQml/qqmlregistration.h>
#include <QObject>

class TreelandWallpaperSurfaceInterfaceV1;
class Workspace;
class TreelandWallpaperNotifierInterfaceV1;
class TreelandWallpaperShellInterfaceV1;
class TreelandUserConfig;

class WallpaperManager : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    enum UpdateReason
    {
        Normal,
        Scale,
    };

    using WallpaperLibrary = QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType>;
    using LibraryProvider = std::function<WallpaperLibrary(wlr_output *)>;
    struct Services {
        LibraryProvider library;
        std::function<const QList<Output *> &()> outputs;
        std::function<TreelandWallpaperNotifierInterfaceV1 *()> notifier;
        TreelandWallpaperShellInterfaceV1 *factory;
    };
    WallpaperManager(TreelandUserConfig *config, Workspace *workspace, Services services,
                     QObject *parent = nullptr);
    ~WallpaperManager() override;
    WallpaperOutputConfig getOutputConfig(Output *output);
    WallpaperOutputConfig getOutputConfig(wlr_output *output);
    WallpaperOutputConfig getOutputConfig(const QString &id);
    void defaultWallpaperConfig();
    void ensureWallpaperConfigForOutput(Output *output);
    bool configContainsOutput(Output *output);
    QString wallpaperConfigToJsonString();
    void setOutputWallpaper(wlr_output *output,
                            int workspaceIndex,
                            const QString &fileSource,
                            TreelandWallpaperInterfaceV1::WallpaperRoles roles,
                            TreelandWallpaperInterfaceV1::WallpaperType type);
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalValidWallpaper(wlr_output *excludedOutput = nullptr);
    void syncAddWorkspace();
    void removeOutputWallpaper(wlr_output *output);
    QString currentWorkspaceWallpaper(WOutput *output);
    QString currentLockScreenWallpaper(WOutput *output);
    TreelandWallpaperInterfaceV1::WallpaperType getWallpaperType(const QString &wallpaper);

Q_SIGNALS:
    void updateWallpaper();

public Q_SLOTS:
    void loadWallpaperConfig();
    void onWallpaperAdded(TreelandWallpaperInterfaceV1 *interface);
    void onImageChanged(int workspaceIndex,
                        const QString &fileSource,
                        TreelandWallpaperInterfaceV1::WallpaperRoles roles);
    void onVideoChanged(int workspaceIndex,
                        const QString &fileSource,
                        TreelandWallpaperInterfaceV1::WallpaperRoles roles);
    void onWallpaperNotifierBound(wl_resource *resource);
    void handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface);

public:
    void refresh();

private:
    TreelandUserConfig *const m_config;
    Workspace *const m_workspace;
    const Services m_services;
    bool m_wallpaperConfigUpdated { false };
    QList<WallpaperOutputConfig> m_wallpaperConfig;
};
