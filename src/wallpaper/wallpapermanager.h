// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "output/output.h"
#include "wallpaper/wallpaperconfig.h"

#include <QObject>

class TreelandWallpaperSurfaceInterfaceV1;

class WallpaperManager : public QObject
{
    Q_OBJECT
public:
    enum UpdateReason
    {
        Normal,
        Scale,
    };

    explicit WallpaperManager(QObject *parent = nullptr);
    ~WallpaperManager() override;
    static QString getOutputId(wlr_output *output);
    static QString getOutputId(Output *output);
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
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalValidWallpaper(wlr_output *exclusiveOutput, int exclusiveworkspaceId);
    void syncAddWorkspace();
    void removeOutputWallpaper(wlr_output *output);
    QString currentWorkspaceWallpaper(WOutput *output);
    TreelandWallpaperInterfaceV1::WallpaperType getWallpaperType(const QString &wallpaper);

Q_SIGNALS:
    void updateWallpaper();

public Q_SLOTS:
    void updateWallpaperConfig();
    void onWallpaperAdded(TreelandWallpaperInterfaceV1 *interface);
    void onImageChanged(int workspaceIndex,
                        const QString &fileSource,
                        TreelandWallpaperInterfaceV1::WallpaperRoles roles);
    void onVideoChanged(int workspaceIndex,
                        const QString &fileSource,
                        TreelandWallpaperInterfaceV1::WallpaperRoles roles);
    void onWallpaperNotifierbinded();
    void handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface);

private:
    bool m_wallpaperConfigUpdated { false };
    QList<WallpaperOutputConfig> m_wallpaperConfig;
};
