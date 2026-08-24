// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/wallpaper/wallpapershellinterfacev1.h"
#include "modules/wallpaper/wallpapernotifierinterfacev1.h"
#include "server-bridge.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
TreelandWallpaperShellInterfaceV1 *g_shell = nullptr;
TreelandWallpaperSurfaceInterfaceV1 *g_wallpaperSurface = nullptr;
TreelandWallpaperNotifierInterfaceV1 *g_notifier = nullptr;
uint32_t g_failedError = 0;
int g_failedCount = 0;
int g_readyCount = 0;
}

void protocol_test_setup(Helper *helper)
{
    g_shell = find_server_interface<TreelandWallpaperShellInterfaceV1>(helper);
    g_notifier = find_server_interface<TreelandWallpaperNotifierInterfaceV1>(helper);
    Q_ASSERT(g_shell && g_notifier);
    QObject::connect(g_shell, &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
                     [](TreelandWallpaperSurfaceInterfaceV1 *surface) {
                         g_wallpaperSurface = surface;
                         QObject::connect(surface, &TreelandWallpaperSurfaceInterfaceV1::beforeDestroy,
                                          [surface]() {
                                              if (g_wallpaperSurface == surface)
                                                  g_wallpaperSurface = nullptr;
                                          });
                         QObject::connect(surface, &TreelandWallpaperSurfaceInterfaceV1::failed,
                                          [](uint32_t error) {
                                              g_failedError = error;
                                              ++g_failedCount;
                                          });
                         QObject::connect(surface, &TreelandWallpaperSurfaceInterfaceV1::ready, []() {
                             ++g_readyCount;
                         });
                     });
}

extern "C" {

void wallpaper_emit_play(void *)
{
    if (g_wallpaperSurface)
        g_wallpaperSurface->setPlay(true);
}

void wallpaper_emit_pause(void *)
{
    if (g_wallpaperSurface)
        g_wallpaperSurface->setPlay(false);
}

void wallpaper_emit_slow_down(void *data)
{
    if (g_wallpaperSurface)
        g_wallpaperSurface->slowDown(500);
    if (data)
        *static_cast<int *>(data) = 1;
}

void wallpaper_notifier_emit_add(void *)
{
    if (g_notifier)
        g_notifier->sendAdd(TreelandWallpaperInterfaceV1::Image, QStringLiteral("/tmp/test-image.jpg"));
}

void wallpaper_notifier_emit_remove(void *)
{
    if (g_notifier)
        g_notifier->sendRemove(QStringLiteral("/tmp/test-image.jpg"));
}

void wallpaper_query_failed(void *data)
{
    *static_cast<int *>(data) = g_failedCount ? static_cast<int>(g_failedError) : -1;
}

void wallpaper_query_failed_count(void *data)
{
    *static_cast<int *>(data) = g_failedCount;
}

void wallpaper_query_ready_count(void *data)
{
    *static_cast<int *>(data) = g_readyCount;
}

void wallpaper_query_wallpaper_ready(void *data)
{
    *static_cast<int *>(data) = (g_wallpaperSurface && g_wallpaperSurface->wallpaperReady()) ? 1 : 0;
}

void wallpaper_query_produced(void *data)
{
    *static_cast<int *>(data) = g_shell ? g_shell->producedWallpapers().size() : -1;
}

} // extern "C"
