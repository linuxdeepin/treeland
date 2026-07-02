// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/wallpaper/wallpapermanagerinterfacev1.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class WallpaperWorkspaceConfig {
public:
    int workspaceId = 0;
    QString desktopWallpaper;
    TreelandWallpaperInterfaceV1::WallpaperType desktopWallpapertype = TreelandWallpaperInterfaceV1::Image;
    bool enable = true;

    QJsonObject toJson() const;

    static WallpaperWorkspaceConfig fromJson(const QJsonObject& obj);
};

class WallpaperOutputConfig {
public:
    QString outputName;
    QString lockscreenWallpaper;
    QList<WallpaperWorkspaceConfig> workspaces;
    TreelandWallpaperInterfaceV1::WallpaperType lockScreenWallpapertype = TreelandWallpaperInterfaceV1::Image;
    bool enable = true;

    QJsonObject toJson() const;
    bool containsWorkspace(int id) const;

    static WallpaperOutputConfig fromJson(const QJsonObject& obj);
};
