// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/wallpaper-color/wallpapercolorinterfacev1.h"
#include "server-bridge.h"

#include <wserver.h>

#include <QString>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WallpaperColorInterfaceV1 *g_manager = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    g_manager = find_server_interface<WallpaperColorInterfaceV1>(helper);
}

extern "C" void wallpaper_color_set_color(const char *output, int is_dark)
{
    if (!g_manager)
        return;
    g_manager->updateWallpaperColor(QString::fromUtf8(output), is_dark != 0);
}
