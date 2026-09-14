// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-alpha-modifier-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wsurface.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>

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

// E-level read: report the captured production SurfaceWrapper's alpha-modifier
// multiplier.  wlroots stores this as a double in [0, 1], updated on commit
// when the client calls wp_alpha_modifier_surface_v1.set_multiplier.
void alpha_modifier_read_server_state(void *data)
{
    auto *state = static_cast<struct alpha_modifier_server_state *>(data);
    state->valid = 0;
    state->has_modifier = 0;
    state->multiplier = 0.0;

    if (!g_wrapper)
        return;

    auto *wsurface = g_wrapper->surface();
    if (!wsurface)
        return;

    state->valid = 1;

    auto *wlr_surf = wsurface->handle();
    if (!wlr_surf)
        return;

    const auto *am_state = wlr_alpha_modifier_v1_get_surface_state(wlr_surf);
    if (am_state) {
        state->has_modifier = 1;
        state->multiplier = am_state->multiplier;
    }
}
