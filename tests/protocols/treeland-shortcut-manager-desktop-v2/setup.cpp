// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"
#include "treeland-shortcut-manager-desktop-v2.h"
#include "workspace/workspace.h"

#include <wbackend.h>
#include <wseat.h>

namespace {
SurfaceWrapper *g_primary = nullptr;
SurfaceWrapper *g_secondary = nullptr;
shortcut_desktop_state g_state {};

WSeat *primarySeat(Helper *helper)
{
    const auto seats = helper->seatManager()->seats();
    return seats.isEmpty() ? nullptr : seats.constFirst();
}

bool hasKeyboardFocus(Helper *helper, SurfaceWrapper *wrapper)
{
    auto *seat = primarySeat(helper);
    return seat && wrapper && seat->keyboardFocusSurface() == wrapper->surface();
}

void refreshWrapperState(Helper *helper)
{
    g_state.wrapper_created = g_primary ? 1 : 0;
    g_state.wrapper_in_workspace =
        (g_primary && helper->workspace()->surfaces().contains(g_primary)) ? 1 : 0;
    g_state.secondary_created = g_secondary ? 1 : 0;
    g_state.secondary_in_workspace =
        (g_secondary && helper->workspace()->surfaces().contains(g_secondary)) ? 1 : 0;
}
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
                         if (!g_primary)
                             g_primary = wrapper;
                         else if (!g_secondary)
                             g_secondary = wrapper;
                         refreshWrapperState(helper);
                     });
}

extern "C" void shortcut_desktop_focus_window(void *data)
{
    auto *state = static_cast<shortcut_desktop_state *>(data);
    auto *helper = Helper::instance();
    if (helper && g_primary)
        helper->activateSurface(g_primary, Qt::ActiveWindowFocusReason);
    *state = g_state;
    if (helper) {
        state->wrapper_visible = g_primary && g_primary->isVisible() ? 1 : 0;
        state->keyboard_focused = hasKeyboardFocus(helper, g_primary) ? 1 : 0;
    }
}

extern "C" void shortcut_desktop_focus_secondary(void *data)
{
    auto *state = static_cast<shortcut_desktop_state *>(data);
    auto *helper = Helper::instance();
    if (helper && g_secondary)
        helper->activateSurface(g_secondary, Qt::ActiveWindowFocusReason);
    *state = g_state;
    if (helper) {
        state->secondary_visible = g_secondary && g_secondary->isVisible() ? 1 : 0;
        state->secondary_focused = hasKeyboardFocus(helper, g_secondary) ? 1 : 0;
    }
}
