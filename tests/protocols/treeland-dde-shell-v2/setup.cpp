// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/dde-shell/ddeshellmanagerinterfacev2.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-dde-shell-v2.h"

#include <wseat.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
DDEShellSurfaceV2 *g_shellSurface = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    auto *manager = find_server_interface<DDEShellManagerInterfaceV2>(helper);
    Q_ASSERT(manager);
    QObject::connect(manager, &DDEShellManagerInterfaceV2::surfaceCreated,
                     [](DDEShellSurfaceV2 *surface) {
                         g_shellSurface = surface;
                         QObject::connect(surface, &QObject::destroyed, [] {
                             g_shellSurface = nullptr;
                         });
                     });
}

extern "C" void dde_shell_v2_query_surface_state(void *data)
{
    auto *state = static_cast<dde_shell_surface_v2_state *>(data);
    *state = {};
    if (!g_shellSurface)
        return;

    state->role_overlay = g_shellSurface->role() == DDEShellSurfaceV2::Role::OVERLAY;
    state->skip_flags = int(g_shellSurface->skipFlags());
    state->accept_keyboard_focus = g_shellSurface->acceptKeyboardFocus();

    if (const auto position = g_shellSurface->positionHint()) {
        state->position_set = 1;
        state->position_x = position->x();
        state->position_y = position->y();
    }
    if (const auto cursor = g_shellSurface->cursorPlacementHint()) {
        state->cursor_set = 1;
        state->cursor_x = cursor->x();
        state->cursor_y = cursor->y();
    }
}
