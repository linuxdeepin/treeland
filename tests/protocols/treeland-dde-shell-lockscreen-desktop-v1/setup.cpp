// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/lockscreen.h"
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-dde-shell-lockscreen-desktop-v1.h"

#include <wbackend.h>

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
}

extern "C" void dde_lockscreen_desktop_read_state(void *data)
{
    dde_lockscreen_desktop_state state {};
    auto *helper = Helper::instance();
    auto *root = helper->rootSurfaceContainer();
    auto *lockScreen = root->findChild<LockScreen *>(QStringLiteral("LockScreenContainer"));

    state.output_ready = !root->outputs().isEmpty() ? 1 : 0;
    state.lockscreen_available = lockScreen && lockScreen->available() ? 1 : 0;
    state.lockscreen_locked = lockScreen && lockScreen->isLocked() ? 1 : 0;
    state.mode_is_normal = helper->currentMode() == Helper::CurrentMode::Normal ? 1 : 0;
    state.mode_is_lockscreen = helper->currentMode() == Helper::CurrentMode::LockScreen ? 1 : 0;
    *static_cast<dde_lockscreen_desktop_state *>(data) = state;
}
