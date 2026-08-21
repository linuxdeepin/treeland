// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-wine-window-state-unstable-v1.h"

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

extern "C" void wine_ws_read_state(void *data)
{
    auto *state = static_cast<wine_ws_state *>(data);
    state->wrapper_created = g_wrapper ? 1 : 0;
    state->minimized = (g_wrapper && g_wrapper->isMinimized()) ? 1 : 0;
    state->attention = (g_wrapper && g_wrapper->attention()) ? 1 : 0;
    // E-level: check QQuickItem visibility -- minimized surface must be hidden
    state->visible = (g_wrapper && g_wrapper->isVisible()) ? 1 : 0;
}

extern "C" void wine_ws_minimize_wrapper(void *)
{
    if (g_wrapper)
        g_wrapper->minimize();
}

extern "C" void wine_ws_unminimize_wrapper(void *)
{
    if (g_wrapper)
        g_wrapper->restoreFromMinimized();
}

extern "C" void wine_ws_set_attention(void *)
{
    if (g_wrapper)
        g_wrapper->setAttention(true);
}

extern "C" void wine_ws_clear_attention(void *)
{
    if (g_wrapper)
        g_wrapper->setAttention(false);
}
