// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperconfig.h"

#define OUTPUT_NAME "outputName"
#define LOCK_SCREEN_WALLAPER_TYPE "lockScreenWallpaperType"
#define DESKTOP_WALLAPER_TYPE "desktopWallpaperType"
#define WORKSPACE_INDEX "workspaceIndex"
#define DESKTOP_WALLPAPER "desktopWallpaper"
#define LOCK_SCREEN_WALLPAPER "lockScreenWallpaper"
#define WORKSPACES "workspaces"
#define ENABLE "enable"

QJsonObject WallpaperWorkspaceConfig::toJson() const
{
    QJsonObject obj;
    obj[WORKSPACE_INDEX] = workspaceId;
    obj[DESKTOP_WALLPAPER] = desktopWallpaper;
    obj[DESKTOP_WALLAPER_TYPE] = desktopWallpapertype;
    obj[ENABLE] = enable;
    return obj;
}

WallpaperWorkspaceConfig WallpaperWorkspaceConfig::fromJson(const QJsonObject &obj)
{
    WallpaperWorkspaceConfig ws;
    ws.workspaceId = obj[WORKSPACE_INDEX].toInt();
    ws.desktopWallpaper = obj[DESKTOP_WALLPAPER].toString();
    ws.desktopWallpapertype = static_cast<TreelandWallpaperInterfaceV1::WallpaperType>(obj[DESKTOP_WALLAPER_TYPE].toInt());
    ws.enable = obj[ENABLE].toBool();
    return ws;
}

QJsonObject WallpaperOutputConfig::toJson() const
{
    QJsonObject obj;
    obj[OUTPUT_NAME] = outputName;
    obj[LOCK_SCREEN_WALLPAPER] = lockscreenWallpaper;
    obj[LOCK_SCREEN_WALLAPER_TYPE] = lockScreenWallpapertype;
    obj[ENABLE] = enable;
    QJsonArray workspacesArray;
    for (const WallpaperWorkspaceConfig& ws : workspaces) {
        workspacesArray.append(ws.toJson());
    }
    obj[WORKSPACES] = workspacesArray;

    return obj;
}

bool WallpaperOutputConfig::containsWorkspace(int id)
{
    for (const WallpaperWorkspaceConfig& ws : workspaces) {
        if (ws.workspaceId == id) {
            return true;
        }
    }

    return false;
}

WallpaperOutputConfig WallpaperOutputConfig::fromJson(const QJsonObject &obj)
{
    WallpaperOutputConfig out;
    out.outputName = obj[OUTPUT_NAME].toString();
    out.lockscreenWallpaper = obj[LOCK_SCREEN_WALLPAPER].toString();
    out.lockScreenWallpapertype = static_cast<TreelandWallpaperInterfaceV1::WallpaperType>(obj[LOCK_SCREEN_WALLAPER_TYPE].toInt());
    out.enable = obj[ENABLE].toBool();
    QJsonArray workspacesArray = obj[WORKSPACES].toArray();
    for (const QJsonValue& value : workspacesArray) {
        out.workspaces.append(WallpaperWorkspaceConfig::fromJson(value.toObject()));
    }

    return out;
}
