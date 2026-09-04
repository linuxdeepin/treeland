// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-viewporter.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wsurface.h>
#include <wlr/types/wlr_compositor.h>

namespace {
// The real mapped XdgToplevel SurfaceWrapper created by the client.
SurfaceWrapper *g_wrapper = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

// E-level read: report the captured production SurfaceWrapper's wl_surface
// viewport destination state.  wp_viewport.set_destination is double-buffered
// state applied on commit, so wlr_surface::current.viewport must reflect the
// client's requested destination rectangle.
void viewporter_read_server_state(void *data)
{
    auto *state = static_cast<struct viewporter_server_state *>(data);
    state->valid = 0;
    state->has_dst = 0;
    state->dst_width = 0;
    state->dst_height = 0;

    if (!g_wrapper)
        return;

    auto *wsurface = g_wrapper->surface();
    if (!wsurface)
        return;

    state->valid = 1;

    auto *wlr_surf = wsurface->handle();
    if (wlr_surf) {
        state->has_dst = wlr_surf->current.viewport.has_dst ? 1 : 0;
        state->dst_width = wlr_surf->current.viewport.dst_width;
        state->dst_height = wlr_surf->current.viewport.dst_height;
    }
}
