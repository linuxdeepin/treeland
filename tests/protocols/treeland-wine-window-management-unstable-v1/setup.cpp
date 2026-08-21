// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-wine-window-management-unstable-v1.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                     });
}

extern "C" void wine_wm_read_state(void *data)
{
    auto *state = static_cast<struct wine_wm_state *>(data);
    state->wrapper_created = g_wrapper ? 1 : 0;
    state->x = g_wrapper ? static_cast<int>(g_wrapper->x()) : 0;
    state->y = g_wrapper ? static_cast<int>(g_wrapper->y()) : 0;
    state->always_on_top = g_wrapper && g_wrapper->alwaysOnTop() ? 1 : 0;
    // E-level: check effective always-on-top and Qt z value
    state->effective_always_on_top = g_wrapper && g_wrapper->effectiveAlwaysOnTop() ? 1 : 0;
    state->z = g_wrapper ? static_cast<int>(g_wrapper->z()) : 0;
    // E-level: surface must still be in the parent container
    state->parent_item_count = g_wrapper && g_wrapper->parentItem()
                                    ? g_wrapper->parentItem()->childItems().size()
                                    : 0;
}
