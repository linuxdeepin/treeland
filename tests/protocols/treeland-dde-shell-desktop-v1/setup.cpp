// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-dde-shell-desktop-v1.h"
#include "workspace/workspace.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
dde_desktop_state g_state {};
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

extern "C" void dde_desktop_read_state(void *data)
{
    auto state = g_state;
    state.output_ready = !Helper::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (g_wrapper) {
        state.is_dde_shell_surface = g_wrapper->isDDEShellSurface() ? 1 : 0;
        state.role_overlay = g_wrapper->surfaceRole() == SurfaceWrapper::SurfaceRole::Overlay ? 1 : 0;
        const QPoint position = g_wrapper->clientRequstPos();
        state.position_x = position.x();
        state.position_y = position.y();
        state.auto_placement = g_wrapper->autoPlaceYOffset();
        state.skip_switcher = g_wrapper->skipSwitcher() ? 1 : 0;
        state.skip_dock_preview = g_wrapper->skipDockPreView() ? 1 : 0;
        state.skip_multitask_view = g_wrapper->skipMutiTaskView() ? 1 : 0;
        state.accept_keyboard_focus = g_wrapper->acceptKeyboardFocus() ? 1 : 0;
    }
    *static_cast<dde_desktop_state *>(data) = state;
}
