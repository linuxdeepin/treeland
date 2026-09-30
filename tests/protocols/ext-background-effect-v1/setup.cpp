// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "ext-background-effect-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>

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

// E-level read: report the captured production SurfaceWrapper's committed
// ext-background-effect-v1 blur region, synced from the protocol surface
// state on every wl_surface.commit.
void background_effect_read_server_state(void *data)
{
    auto *state = static_cast<struct background_effect_server_state *>(data);
    state->valid = 0;
    state->has_region = 0;
    state->blur_enabled = 0;
    state->x = 0;
    state->y = 0;
    state->width = 0;
    state->height = 0;

    if (!g_wrapper)
        return;

    state->valid = 1;
    const QRegion region = g_wrapper->blurRegion();
    state->blur_enabled = g_wrapper->blur() ? 1 : 0;
    if (region.isEmpty())
        return;

    state->has_region = 1;
    const QRect rect = region.boundingRect();
    state->x = rect.x();
    state->y = rect.y();
    state->width = rect.width();
    state->height = rect.height();
}
