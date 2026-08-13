// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/wallpaper-color/wallpapercolorinterfacev1.h"

#include <wserver.h>

#include <QString>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WallpaperColorInterfaceV1 *g_manager = nullptr;
}

void protocol_test_setup(WServer *server)
{
    g_manager = server->attach<WallpaperColorInterfaceV1>();
}

/*
 * Server-side stimulus: the protocol has no client request that changes the
 * wallpaper color; the compositor pushes colors through the Q_INVOKABLE
 * WallpaperColorInterfaceV1::updateWallpaperColor() (invoked by the shell in
 * production). Calling it here is deliberate server-side stimulus, not a
 * faked request result: the client observes the real output_color events it
 * triggers.
 */
extern "C" void wallpaper_color_set_color(const char *output, int is_dark)
{
    if (!g_manager)
        return;
    g_manager->updateWallpaperColor(QString::fromUtf8(output), is_dark != 0);
}
