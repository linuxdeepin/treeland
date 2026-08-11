// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "protocol-test-server.h"
#include "treeland-wallpaper-manager-unstable-v1.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
TreelandWallpaperInterfaceV1 *g_wallpaper = nullptr;
TreelandWallpaperInterfaceV1 *g_wallpaper2 = nullptr;

}

void protocol_test_setup(WServer *server)
{
    protocol_test_create_headless_output(server);

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
