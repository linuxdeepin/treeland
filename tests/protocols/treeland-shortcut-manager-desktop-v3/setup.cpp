// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"
#include "treeland-shortcut-manager-desktop-v3.h"
#include "workspace/workspace.h"

#include <wbackend.h>
#include <wseat.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
shortcut_desktop_state g_state {};

WSeat *primarySeat(WaylandServer *server)
{
    const auto seats = server->seatManager()->seats();
    return seats.isEmpty() ? nullptr : seats.constFirst();
}

bool hasKeyboardFocus(WaylandServer *server)
{
    auto *seat = primarySeat(server);
    return seat && g_wrapper && seat->keyboardFocusSurface() == g_wrapper->surface();
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [server](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = server->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void shortcut_desktop_focus_window(void *data)
{
    auto *state = static_cast<shortcut_desktop_state *>(data);
    auto *server = WaylandServer::instance();
    if (server && g_wrapper)
        server->activateSurface(g_wrapper, Qt::ActiveWindowFocusReason);
    *state = g_state;
    if (server) {
        state->wrapper_visible = g_wrapper && g_wrapper->isVisible() ? 1 : 0;
        state->keyboard_focused = hasKeyboardFocus(server) ? 1 : 0;
    }
}
