// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-screensaver-desktop-v1.h"
#include "workspace/workspace.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
screensaver_desktop_state g_state {};
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void screensaver_desktop_read_state(void *data)
{
    auto state = g_state;
    if (g_wrapper)
        state.wrapper_visible = g_wrapper->isVisible() ? 1 : 0;
    *static_cast<screensaver_desktop_state *>(data) = state;
}
