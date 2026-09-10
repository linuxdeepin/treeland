// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-single-pixel-buffer-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wsurface.h>
#include <wlr/types/wlr_compositor.h>

namespace {
// The real mapped XdgToplevel SurfaceWrapper created by the client's
// xdg_toplevel_client_complete_map().  Captured from the production
// ShellHandler::surfaceWrapperAdded signal.
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
// buffer dimensions.  A single-pixel buffer is 1×1, so wlr_surface::current
// must report buffer_width == 1 and buffer_height == 1.
void single_pixel_buffer_read_server_state(void *data)
{
    auto *state = static_cast<struct single_pixel_buffer_server_state *>(data);
    state->valid = 0;
    state->mapped = 0;
    state->buffer_width = 0;
    state->buffer_height = 0;

    if (!g_wrapper)
        return;

    auto *wsurface = g_wrapper->surface();
    if (!wsurface)
        return;

    state->valid = 1;
    state->mapped = wsurface->mapped() ? 1 : 0;

    auto *wlr_surf = wsurface->handle();
    if (wlr_surf) {
        state->buffer_width = wlr_surf->current.buffer_width;
        state->buffer_height = wlr_surf->current.buffer_height;
    }
}
