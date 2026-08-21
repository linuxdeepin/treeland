// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-dde-shell-v1.h"

#include <wseat.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WindowOverlapCheckerInterface *g_checker = nullptr;
DDEActiveInterface *g_active = nullptr;
WindowPickerInterface *g_picker = nullptr;
DDEShellSurfaceInterface *g_shellSurface = nullptr;

}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    auto *manager = find_server_interface<DDEShellManagerInterfaceV1>(helper);
    Q_ASSERT(manager);
    QObject::connect(manager, &DDEShellManagerInterfaceV1::windowOverlapCheckerCreated,
                     [](WindowOverlapCheckerInterface *checker) { g_checker = checker; });
    QObject::connect(manager, &DDEShellManagerInterfaceV1::activeCreated,
                     [](DDEActiveInterface *active) { g_active = active; });
    QObject::connect(manager, &DDEShellManagerInterfaceV1::PickerCreated,
                     [](WindowPickerInterface *picker) { g_picker = picker; });
    QObject::connect(manager, &DDEShellManagerInterfaceV1::surfaceCreated,
                     [](DDEShellSurfaceInterface *surface) { g_shellSurface = surface; });
}

extern "C" void dde_shell_query_surface_state(void *data)
{
    auto *state = static_cast<dde_shell_surface_state *>(data);
    *state = {};
    if (!g_shellSurface)
        return;

    if (const auto position = g_shellSurface->surfacePos()) {
        state->position_x = position->x();
        state->position_y = position->y();
    }
    state->role_overlay = g_shellSurface->role()
                          == DDEShellSurfaceInterface::Role::OVERLAY;
    state->auto_placement = g_shellSurface->yOffset().value_or(0);
    state->skip_switcher = g_shellSurface->skipSwitcher().value_or(false);
    state->skip_dock_preview = g_shellSurface->skipDockPreView().value_or(false);
    state->skip_multitask_view = g_shellSurface->skipMutiTaskView().value_or(false);
    state->accept_keyboard_focus = g_shellSurface->acceptKeyboardFocus();
}

extern "C" void dde_shell_emit_test_events(void *)
{
    if (g_checker) {
        g_checker->sendOverlapped(true);
        g_checker->sendOverlapped(false);
    }
    if (g_active) {
        DDEActiveInterface::sendActiveIn(0, g_active->seat());
        DDEActiveInterface::sendActiveOut(1, g_active->seat());
        DDEActiveInterface::sendStartDrag(g_active->seat());
        DDEActiveInterface::sendDrop(g_active->seat());
    }
    if (g_picker)
        g_picker->sendWindowPid(42);
}
