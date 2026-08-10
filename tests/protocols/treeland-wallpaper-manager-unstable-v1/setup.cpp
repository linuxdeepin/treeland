// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "treeland-wallpaper-manager-unstable-v1.h"

#include <wbackend.h>
#include <wserver.h>

#include <qwbackend.h>
#include <qwdisplay.h>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

namespace {
TreelandWallpaperInterfaceV1 *g_wallpaper = nullptr;
TreelandWallpaperInterfaceV1 *g_wallpaper2 = nullptr;


void createTestOutput(WServer *server)
{
    auto *backend = server->findInterface<WBackend>();
    if (!backend)
        return;


    backend->handle()->start();

    auto *multi = qw_multi_backend::from(backend->handle()->handle());
    if (!multi)
        return;
    wlr_backend *headlessHandle = nullptr;
    multi->for_each_backend([](wlr_backend *b, void *data) {
        if (wlr_backend_is_headless(b))
            *static_cast<wlr_backend **>(data) = b;
    }, &headlessHandle);
    if (!headlessHandle)
        return;
    auto *headless = qw_headless_backend::from(headlessHandle);
    auto *output = headless->add_output(1920, 1080);
    if (!output)
        return;


    wlr_output_create_global(output, server->handle()->handle());
}
}

void protocol_test_setup(WServer *server)
{
    createTestOutput(server);

    auto *manager = server->attach<TreelandWallpaperManagerInterfaceV1>();
    QObject::connect(manager, &TreelandWallpaperManagerInterfaceV1::wallpaperCreated,
                     [](TreelandWallpaperInterfaceV1 *wallpaper) {
                         if (!g_wallpaper)
                             g_wallpaper = wallpaper;
                         else if (!g_wallpaper2)
                             g_wallpaper2 = wallpaper;
                     });
}

extern "C" void wm_query_server_state(void *data)
{
    auto *state = static_cast<struct wm_server_state *>(data);
    state->wallpaper_created = g_wallpaper != nullptr;
    state->second_created = g_wallpaper2 != nullptr;
    state->output_valid = g_wallpaper && g_wallpaper->wOutput() != nullptr;
    state->has_username = g_wallpaper && !g_wallpaper->userName().isEmpty();
}

extern "C" void wm_emit_failed(void *)
{
    if (g_wallpaper) {
        g_wallpaper->sendError(WM_TEST_SOURCE, TreelandWallpaperInterfaceV1::InvalidSource);
    }
}

extern "C" void wm_emit_changed(void *)
{
    if (g_wallpaper) {
        const auto roles = static_cast<TreelandWallpaperInterfaceV1::WallpaperRole>(
            TreelandWallpaperInterfaceV1::Desktop | TreelandWallpaperInterfaceV1::Lockscreen);
        g_wallpaper->sendChanged(roles, TreelandWallpaperInterfaceV1::Image, WM_TEST_SOURCE);
    }
}
