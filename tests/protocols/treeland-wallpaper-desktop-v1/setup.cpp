// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "modules/wallpaper/wallpapershellinterfacev1.h"
#include "protocol-test-server.h"
#include "seat/helper.h"
#include "treeland-wallpaper-desktop-v1.h"
#include "wallpaper/wallpaperswitcheritem.h"

#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>

namespace {
constexpr auto kWallpaperSource = "/tmp/treeland-protocol-wallpaper-red";

WallpaperSwitcherItem *findWallpaperSwitcher(QQuickItem *item,
                                              WOutput *output,
                                              const QString &source)
{
    if (auto *switcher = qobject_cast<WallpaperSwitcherItem *>(item);
        switcher && switcher->output() == output && switcher->source() == source) {
        return switcher;
    }
    for (auto *child : item->childItems()) {
        if (auto *switcher = findWallpaperSwitcher(child, output, source))
            return switcher;
    }
    return nullptr;
}

WSurfaceItemContent *findSurfaceContent(QQuickItem *item, WSurface *surface)
{
    if (auto *content = qobject_cast<WSurfaceItemContent *>(item);
        content && content->surface() == surface) {
        return content;
    }
    for (auto *child : item->childItems()) {
        if (auto *content = findSurfaceContent(child, surface))
            return content;
    }
    return nullptr;
}
}

void protocol_test_desktop_setup([[maybe_unused]] Helper *helper)
{
    // The desktop fixture already owns one headless output.  Keeping that
    // single output makes the client's wl_output and the active QML output
    // unambiguous for this manager + shell integration path.
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
    state.shell_surface_mapped = surface && surface->mapped() ? 1 : 0;
    state.shell_surface_ready = shellSurface->wallpaperReady() ? 1 : 0;
    if (surface) {
        const QSize size = surface->size();
        state.surface_width = size.width();
        state.surface_height = size.height();
    }

    auto *managerWallpaper =
        TreelandWallpaperInterfaceV1::getReferenceWallpaperInterfaceFromSurface(surface);
    state.manager_reference_matched = managerWallpaper ? 1 : 0;
    auto *output = managerWallpaper ? managerWallpaper->wOutput() : nullptr;
    state.output_matched = output ? 1 : 0;
    state.manager_configured = output
            && helper->currentWorkspaceWallpaper(output) == QString::fromLatin1(kWallpaperSource)
        ? 1
        : 0;

    if (auto *switcher = findWallpaperSwitcher(helper->window()->contentItem(),
                                               output,
                                               QString::fromLatin1(kWallpaperSource))) {
        state.switcher_source_matched = 1;
        if (auto *content = findSurfaceContent(switcher, surface)) {
            state.content_surface_matched = 1;
            state.content_visible = content->isVisible() ? 1 : 0;
        }
    }

    *static_cast<wallpaper_desktop_state *>(data) = state;
}
