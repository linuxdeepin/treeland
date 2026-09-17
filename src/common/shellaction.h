// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <cstdint>

// ShellAction is the compositor-neutral vocabulary both treeland-compositor-
// action-v1 and treeland-shortcut-manager-v2 map their own action enums onto,
// so each action is implemented once. The protocol enums are independent value
// spaces; this enum is the shared translation target.
enum class ShellAction : uint32_t {
    // Sentinel so a default-constructed ShellAction{} is never a real action.
    Invalid = 0,

    // Contiguous: the workspace index is action - SwitchWorkspace1.
    SwitchWorkspace1,
    SwitchWorkspace2,
    SwitchWorkspace3,
    SwitchWorkspace4,
    SwitchWorkspace5,
    SwitchWorkspace6,
    SwitchWorkspace7,
    SwitchWorkspace8,
    SwitchWorkspace9,
    SwitchWorkspace10,
    SwitchWorkspace11,
    SwitchWorkspace12,
    PreviousWorkspace,
    NextWorkspace,
    ToggleShowDesktop,
    OpenMultitaskView,
    CloseMultitaskView,
    ToggleMultitaskView,
    ToggleFpsDisplay,
    ZoomIn,
    ZoomOut,
    ZoomReset,
    LockScreen,
    ShowShutdownMenu,
    ShowShutdownMenuPowerOff,
    ShowShutdownMenuReboot,
    ShowShutdownMenuSuspend,
    ShowShutdownMenuHibernate,
    ShowShutdownMenuLogOut,
    ShowUserSwitch,

    // Window-level actions (shortcut-manager only); act on activatedSurface().
    Maximize,
    CancelMaximize,
    MoveWindow,
    CloseWindow,
    ShowWindowMenu,
    TileLeft,
    TileRight,
};

// Stateless executor: every call re-reads Helper::instance(). Task switching,
// Notify and Quit stay producer-local (they depend on switch/quit state).
class ShellActionExecutor
{
public:
    static void execute(ShellAction action);
};
