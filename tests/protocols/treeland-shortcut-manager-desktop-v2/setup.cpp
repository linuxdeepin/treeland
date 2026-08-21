// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "surface/surfacewrapper.h"
#include "treeland-shortcut-manager-desktop-v2.h"
#include "workspace/workspace.h"

#include <wbackend.h>
#include <wseat.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
shortcut_desktop_state g_state {};

WSeat *primarySeat(Helper *helper)
{
    const auto seats = helper->seatManager()->seats();
    return seats.isEmpty() ? nullptr : seats.constFirst();
}

bool hasKeyboardFocus(Helper *helper)
{
    auto *seat = primarySeat(helper);
    return seat && g_wrapper && seat->keyboardFocusSurface() == g_wrapper->surface();
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
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void shortcut_desktop_focus_window(void *data)
{
    auto *state = static_cast<shortcut_desktop_state *>(data);
    auto *helper = Helper::instance();
    if (helper && g_wrapper)
        helper->activateSurface(g_wrapper, Qt::ActiveWindowFocusReason);
    *state = g_state;
    if (helper) {
        state->wrapper_visible = g_wrapper && g_wrapper->isVisible() ? 1 : 0;
        state->keyboard_focused = hasKeyboardFocus(helper) ? 1 : 0;
    }
}
