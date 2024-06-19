// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercolor.h"

#include "impl/wallpaper_color_impl.h"

#include <woutput.h>
#include <wserver.h>

#include <qwdisplay.h>

#include <QDebug>
#include <QQmlInfo>

extern "C" {
#include <wayland-server-core.h>
}

TreelandWallpaperColor::TreelandWallpaperColor(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
{
}

WServerInterface *TreelandWallpaperColor::create()
{
    m_handle = wallpaper_color_manager_v1::create(server()->handle());

    return new WServerInterface(m_handle, m_handle->global);
}

void TreelandWallpaperColor::updateWallpaperColor(const QString &output, bool isDarkType)
{
    m_handle->updateWallpaperColor(output, isDarkType);
}
