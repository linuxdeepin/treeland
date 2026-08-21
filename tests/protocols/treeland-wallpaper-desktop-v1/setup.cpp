// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "modules/wallpaper/wallpapershellinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treelanduserconfig.hpp"
#include "treeland-wallpaper-desktop-v1.h"
namespace {
constexpr auto kWallpaperSource = "/tmp/treeland-protocol-wallpaper-red";
}

void protocol_test_setup([[maybe_unused]] Helper *helper)
{
    // The desktop fixture already owns one headless output.  Keeping that
    // single output makes the client's wl_output and the active QML output
    // unambiguous for this manager + shell integration path.
}

extern "C" bool protocol_test_ready(Helper *helper)
{
    auto *config = helper->config();
#if TREELANDUSERCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    return config && config->isInitializeSucceeded();
#else
    return config && config->isInitializeSucceed();
#endif
}

extern "C" void wallpaper_desktop_read_state(void *data)
{
    wallpaper_desktop_state state {};
    auto *helper = Helper::instance();
    auto *shellSurface = TreelandWallpaperSurfaceInterfaceV1::get(
        QString::fromLatin1(kWallpaperSource));
    state.shell_surface_registered = shellSurface ? 1 : 0;
    if (!shellSurface) {
        *static_cast<wallpaper_desktop_state *>(data) = state;
        return;
    }

    auto *surface = shellSurface->wSurface();

    auto *managerWallpaper =
        TreelandWallpaperInterfaceV1::getReferenceWallpaperInterfaceFromSurface(surface);
    state.manager_reference_matched = managerWallpaper ? 1 : 0;
    auto *output = managerWallpaper ? managerWallpaper->wOutput() : nullptr;
    state.output_matched = output ? 1 : 0;
    state.manager_configured = output
            && helper->currentWorkspaceWallpaper(output) == QString::fromLatin1(kWallpaperSource)
        ? 1
        : 0;

    *static_cast<wallpaper_desktop_state *>(data) = state;
}
