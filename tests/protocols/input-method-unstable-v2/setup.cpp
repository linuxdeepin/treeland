// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "server-bridge.h"
#include "server-bridge-api.h"
#include "core/waylandserver.h"
#include "core/shellhandler.h"
#include "rootsurfacecontainer.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <winputmethodhelper.h>
#include <wseat.h>

namespace {
struct ActivateToplevelData {
    int index = 0;
    bool activated = false;
};

void activateToplevel(void *data)
{
    auto *request = static_cast<ActivateToplevelData *>(data);
    auto *server = WaylandServer::instance();
    auto *seat = server ? server->seat() : nullptr;
    if (!server || !seat)
        return;

    int index = 0;
    const auto surfaces = server->rootSurfaceContainer()->surfaces();
    for (auto *wrapper : surfaces) {
        if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
            continue;
        if (index++ != request->index)
            continue;
        server->activateSurface(wrapper, Qt::ActiveWindowFocusReason, seat);
        request->activated = true;
        return;
    }
}

// Exercises the suspension invariant on the live helper: once deactivate() has
// run (what a session switch does), a focus change arriving afterwards must not
// bring text-input routing or an input-method composition back. A closing
// popup, a late commit or a window switch used to re-run the text-input
// reconciliation and re-activate the session that was no longer on screen.
struct SuspensionData {
    bool suspendedAfterDeactivate = false;
    bool activeAfterDeactivate = false;
    bool suspendedAfterFocusChurn = false;
    bool activeAfterFocusChurn = false;
    bool focusSurfaceWhileSuspended = false;
    bool wokeUpOnActivate = false;
    bool hadHelper = false;
};

void checkSuspension(void *data)
{
    auto *request = static_cast<SuspensionData *>(data);
    auto *server = WaylandServer::instance();
    auto *seat = server ? server->seat() : nullptr;
    if (!server || !seat)
        return;

    auto *helper = server->shellHandler()->inputMethodHelper(seat);
    if (!helper)
        return;
    request->hadHelper = true;

    helper->deactivate();
    request->suspendedAfterDeactivate = helper->isSuspended();
    request->activeAfterDeactivate = helper->inputMethodActive();

    // Churn the focus the way a closing popup or a window switch would: clear
    // and restore the keyboard focus. With the session suspended this must not
    // bring text-input routing or an input method back.
    server->activateSurface(nullptr, Qt::OtherFocusReason, seat);
    const auto surfaces = server->rootSurfaceContainer()->surfaces();
    for (auto *wrapper : surfaces) {
        if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
            continue;
        server->activateSurface(wrapper, Qt::ActiveWindowFocusReason, seat);
        break;
    }
    request->suspendedAfterFocusChurn = helper->isSuspended();
    request->activeAfterFocusChurn = helper->inputMethodActive();
    request->focusSurfaceWhileSuspended = helper->textInputFocusSurface() != nullptr;

    // Leave the helper in a clean state for the remaining protocol steps.
    helper->activate();
    request->wokeUpOnActivate = !helper->isSuspended();
}
}

extern "C" int protocol_test_activate_toplevel(int index)
{
    ActivateToplevelData data;
    data.index = index;
    if (!invoke_on_server_thread(activateToplevel, &data))
        return 0;
    return data.activated ? 1 : 0;
}

extern "C" int protocol_test_input_method_suspension(void)
{
    SuspensionData data;
    if (!invoke_on_server_thread(checkSuspension, &data))
        return 0;
    if (!data.hadHelper)
        return 0;
    // The latch holds, no composition is active after deactivate(), the focus
    // churn changes neither, and a suspended helper reports no text-input focus
    // anchor even though a focused text input still exists. activate() clears
    // the latch again.
    return (data.suspendedAfterDeactivate && !data.activeAfterDeactivate
            && data.suspendedAfterFocusChurn && !data.activeAfterFocusChurn
            && !data.focusSurfaceWhileSuspended && data.wokeUpOnActivate)
        ? 1
        : 0;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
}
