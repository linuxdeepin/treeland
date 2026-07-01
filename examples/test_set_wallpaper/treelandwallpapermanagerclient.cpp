// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandwallpapermanagerclient.h"

#include <QDebug>

TreelandWallpaperManagerV1::TreelandWallpaperManagerV1()
    : QWaylandClientExtensionTemplate<TreelandWallpaperManagerV1>(InterfaceVersion)
    , QtWayland::treeland_wallpaper_manager_v1()
{
}

TreelandWallpaperManagerV1::~TreelandWallpaperManagerV1()
{
    destroy();
}

void TreelandWallpaperManagerV1::instantiate()
{
    initialize();
}

TreelandWallpaperV1::TreelandWallpaperV1(struct ::treeland_wallpaper_v1 *object)
    : QtWayland::treeland_wallpaper_v1(object)
{
}

TreelandWallpaperV1::~TreelandWallpaperV1()
{
    destroy();
}

void TreelandWallpaperV1::treeland_wallpaper_v1_failed(const QString &source, uint32_t error)
{
    qWarning() << "treeland_wallpaper_v1_failed:" << source << error;
}

void TreelandWallpaperV1::treeland_wallpaper_v1_changed(uint32_t role,
                                                        uint32_t sourceType,
                                                        const QString &fileSource)
{
    qWarning() << "treeland_wallpaper_v1_changed, role:"
               << role << ", source type:"
               << sourceType << ", file:"
               << fileSource;
}
