// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercolor.h"

#include "modules/wallpaper-color/impl/wallpaper_color_impl.h"

#include <woutput.h>
#include <wserver.h>

#include <qwdisplay.h>

#include <QDebug>
#include <QQmlInfo>

WallpaperColorV1::WallpaperColorV1(QObject *parent)
    : QObject(parent)
{
}

void WallpaperColorV1::create(WServer *server)
{
    m_handle = wallpaper_color_manager_v1::create(server->handle());
}

void WallpaperColorV1::destroy(WServer *server) { }

wl_global *WallpaperColorV1::global() const
{
    return m_handle->global;
}

void WallpaperColorV1::updateWallpaperColor(const QString &output, bool isDarkType)
{
    m_handle->updateWallpaperColor(output, isDarkType);
}

QByteArrayView WallpaperColorV1::interfaceName() const
{
    return treeland_wallpaper_color_manager_v1_interface.name;
}
