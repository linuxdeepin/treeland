// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-wallpaper-manager-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtension>

class TreelandWallpaperV1;

class TreelandWallpaperManagerV1
    : public QWaylandClientExtensionTemplate<TreelandWallpaperManagerV1>
    , public QtWayland::treeland_wallpaper_manager_v1
{
    Q_OBJECT
public:
    static constexpr int InterfaceVersion = 1;
    explicit TreelandWallpaperManagerV1();
    ~TreelandWallpaperManagerV1() override;

    void instantiate();
};

class TreelandWallpaperV1 : public QtWayland::treeland_wallpaper_v1
{
public:
    explicit TreelandWallpaperV1(struct ::treeland_wallpaper_v1 *object);
    ~TreelandWallpaperV1() override;

protected:
    void treeland_wallpaper_v1_failed(const QString &source, uint32_t error) override;
    void treeland_wallpaper_v1_changed(uint32_t role,
                                       uint32_t sourceType,
                                       const QString &fileSource) override;
};
