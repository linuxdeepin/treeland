// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "input/gestures.h"
#include "modules/shortcut/qwayland-server-treeland-shortcut-manager-unstable-v3.h"

#include <QMap>
#include <QKeyCombination>

class Gesture;
class QKeyEvent;

class ShortcutController : public QObject
{
    Q_OBJECT
public:
    // Internal action vocabulary. Values are taken directly from the
    // treeland-shortcut-manager-unstable-v3 protocol enum so they cannot
    // drift; the PascalCase names keep the Qt-facing API readable.
    enum class ShortcutAction : uint32_t {
        Notify                    = QtWaylandServer::treeland_shortcut_manager_v3::action_notify,
        Workspace1                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_1,
        Workspace2                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_2,
        Workspace3                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_3,
        Workspace4                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_4,
        Workspace5                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_5,
        Workspace6                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_6,
        Workspace7                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_7,
        Workspace8                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_8,
        Workspace9                = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_9,
        Workspace10               = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_10,
        Workspace11               = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_11,
        Workspace12               = QtWaylandServer::treeland_shortcut_manager_v3::action_workspace_12,
        PrevWorkspace             = QtWaylandServer::treeland_shortcut_manager_v3::action_prev_workspace,
        NextWorkspace             = QtWaylandServer::treeland_shortcut_manager_v3::action_next_workspace,
        Maximize                  = QtWaylandServer::treeland_shortcut_manager_v3::action_maximize,
        CancelMaximize            = QtWaylandServer::treeland_shortcut_manager_v3::action_cancel_maximize,
        Minimize                  = QtWaylandServer::treeland_shortcut_manager_v3::action_minimize,
        MoveWindow                = QtWaylandServer::treeland_shortcut_manager_v3::action_move_window,
        ResizeWindow              = QtWaylandServer::treeland_shortcut_manager_v3::action_resize_window,
        CloseWindow               = QtWaylandServer::treeland_shortcut_manager_v3::action_close_window,
        ShowWindowMenu            = QtWaylandServer::treeland_shortcut_manager_v3::action_show_window_menu,
        MoveWindowToPrevWorkspace = QtWaylandServer::treeland_shortcut_manager_v3::action_move_window_to_prev_workspace,
        MoveWindowToNextWorkspace = QtWaylandServer::treeland_shortcut_manager_v3::action_move_window_to_next_workspace,
        ShowDesktop               = QtWaylandServer::treeland_shortcut_manager_v3::action_show_desktop,
        OpenMultiTaskView         = QtWaylandServer::treeland_shortcut_manager_v3::action_open_multitask_view,
        CloseMultiTaskView        = QtWaylandServer::treeland_shortcut_manager_v3::action_close_multitask_view,
        ToggleMultitaskView       = QtWaylandServer::treeland_shortcut_manager_v3::action_toggle_multitask_view,
        TaskSwitchNext            = QtWaylandServer::treeland_shortcut_manager_v3::action_taskswitch_next,
        TaskSwitchPrev            = QtWaylandServer::treeland_shortcut_manager_v3::action_taskswitch_prev,
        TaskSwitchSameAppNext     = QtWaylandServer::treeland_shortcut_manager_v3::action_taskswitch_sameapp_next,
        TaskSwitchSameAppPrev     = QtWaylandServer::treeland_shortcut_manager_v3::action_taskswitch_sameapp_prev,
        TileLeft                  = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_left,
        TileRight                 = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_right,
        TileTop                   = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_top,
        TileBottom                = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_bottom,
        TileTopLeft               = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_top_left,
        TileTopRight              = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_top_right,
        TileBottomLeft            = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_bottom_left,
        TileBottomRight           = QtWaylandServer::treeland_shortcut_manager_v3::action_tile_bottom_right,
        ZoomIn                    = QtWaylandServer::treeland_shortcut_manager_v3::action_zoom_in,
        ZoomOut                   = QtWaylandServer::treeland_shortcut_manager_v3::action_zoom_out,
        ZoomReset                 = QtWaylandServer::treeland_shortcut_manager_v3::action_zoom_reset,
        ToggleFpsDisplay          = QtWaylandServer::treeland_shortcut_manager_v3::action_toggle_fps_display,
        Lockscreen                = QtWaylandServer::treeland_shortcut_manager_v3::action_lockscreen,
        ShutdownMenu              = QtWaylandServer::treeland_shortcut_manager_v3::action_shutdown_menu,
    };
    Q_ENUM(ShortcutAction)
    static const char *actionName(ShortcutAction action);

    enum KeyFlag : uint32_t {
        None = 0,
        KeyPress = 0x1,
        KeyRelease = 0x2,
        Repeat = 0x4,
        All = KeyPress | KeyRelease | Repeat
    };
    Q_DECLARE_FLAGS(KeyFlags, KeyFlag)
    explicit ShortcutController(QObject *parent = nullptr);
    ~ShortcutController() override;

    uint registerKey(const QString &name, const QString& key, KeyFlags keybindFlags, ShortcutAction action);
    uint registerSwipeGesture(const QString &name, uint finger, SwipeGesture::Direction direction, ShortcutAction action);
    uint registerHoldGesture(const QString &name, uint finger, ShortcutAction action);
    void unregisterShortcut(const QString &name);

    void clear();
    bool dispatchKeyEvent(const QKeyEvent *event);
    static QKeyCombination normalizeKeyCombination(QKeyCombination combination);
    static bool isValidShortcutCombination(QKeyCombination combination);
    // Normalizes a portable-text key string to the (combined-key) identity
    // registerKey() upserts on, or 0 when the string does not parse to exactly
    // one combination. Single source of truth for the bind_key cache identity.
    static int normalizedKeyCombinedFor(const QString &key);
    Qt::KeyboardModifiers modifierForAction(ShortcutAction action) const;

Q_SIGNALS:
    void actionTriggered(ShortcutAction action, const QString &name, bool isGesture, KeyFlags keyFlags = {});
    void actionProgress(ShortcutAction action, qreal progress, const QString &name);
    void actionFinished(ShortcutAction action, const QString &name, bool isTriggered);

private:

    QMap<int, QMap<ShortcutAction, std::pair<QString, KeyFlags>>> m_keyMap;
    QMap<std::pair<uint, SwipeGesture::Direction>, QMap<ShortcutAction, QString>> m_gesturemap;
    QMap<std::pair<uint, SwipeGesture::Direction>, QObject*> m_gestures;
    QMap<QString, std::function<void()>> m_deleters;
    QMap<ShortcutAction, int> m_actionCombinedMap;
};

// Convenience alias so existing bare `ShortcutAction` references keep working
using ShortcutAction = ShortcutController::ShortcutAction;

Q_DECLARE_OPERATORS_FOR_FLAGS(ShortcutController::KeyFlags)
