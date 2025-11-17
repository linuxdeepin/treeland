// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include "modules/shortcut/impl/shortcut_manager_impl.h"
#include "treeland-shortcut-manager-protocol.h"

#include <QObject>
#include <QQmlEngine>

class ShortcutController;
class ShortcutManagerV2Private;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

enum class ShortcutAction {
    Notify = TREELAND_SHORTCUT_MANAGER_V2_ACTION_NOTIFY,
    Workspace1 = TREELAND_SHORTCUT_MANAGER_V2_ACTION_WORKSPACE_1,
    Workspace2 = TREELAND_SHORTCUT_MANAGER_V2_ACTION_WORKSPACE_2,
    Workspace3 = TREELAND_SHORTCUT_MANAGER_V2_ACTION_WORKSPACE_3,
    Workspace4 = TREELAND_SHORTCUT_MANAGER_V2_ACTION_WORKSPACE_4,
    Workspace5 = TREELAND_SHORTCUT_MANAGER_V2_ACTION_WORKSPACE_5,
    Workspace6 = TREELAND_SHORTCUT_MANAGER_V2_ACTION_WORKSPACE_6,
    PrevWorkspace = TREELAND_SHORTCUT_MANAGER_V2_ACTION_PREV_WORKSPACE,
    NextWorkspace = TREELAND_SHORTCUT_MANAGER_V2_ACTION_NEXT_WORKSPACE,
    ShowDesktop = TREELAND_SHORTCUT_MANAGER_V2_ACTION_SHOW_DESKTOP,
    Maximize = TREELAND_SHORTCUT_MANAGER_V2_ACTION_MAXIMIZE,
    CancelMaximize = TREELAND_SHORTCUT_MANAGER_V2_ACTION_CANCEL_MAXIMIZE,
    MoveWindow = TREELAND_SHORTCUT_MANAGER_V2_ACTION_MOVE_WINDOW,
    CloseWindow = TREELAND_SHORTCUT_MANAGER_V2_ACTION_CLOSE_WINDOW,
    ShowWindowMenu = TREELAND_SHORTCUT_MANAGER_V2_ACTION_SHOW_WINDOW_MENU,
    OpenMultiTaskView = TREELAND_SHORTCUT_MANAGER_V2_ACTION_OPEN_MULTITASK_VIEW,
    CloseMultiTaskView = TREELAND_SHORTCUT_MANAGER_V2_ACTION_CLOSE_MULTITASK_VIEW,
    ToggleMultitaskView = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TOGGLE_MULTITASK_VIEW,
    ToggleFpsDisplay = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TOGGLE_FPS_DISPLAY,
    Lockscreen = TREELAND_SHORTCUT_MANAGER_V2_ACTION_LOCKSCREEN,
    ShutdownMenu = TREELAND_SHORTCUT_MANAGER_V2_ACTION_SHUTDOWN_MENU,
    Quit = TREELAND_SHORTCUT_MANAGER_V2_ACTION_QUIT,
    TaskSwitchEnter = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TASKSWITCH_ENTER,
    TaskSwitchNext = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TASKSWITCH_NEXT,
    TaskSwitchPrev = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TASKSWITCH_PREV,
    TaskSwitchSameAppNext = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TASKSWITCH_SAMEAPP_NEXT,
    TaskSwitchSameAppPrev = TREELAND_SHORTCUT_MANAGER_V2_ACTION_TASKSWITCH_SAMEAPP_PREV,
};

class ShortcutManagerV2
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ShortcutManagerV2)

public:
    explicit ShortcutManagerV2(QObject *parent = nullptr);
    ~ShortcutManagerV2() override;
    QByteArrayView interfaceName() const override;

    ShortcutController* controller();
    void sendActivated(const QString& name, bool repeat = false);

public Q_SLOTS:
    void onSessionChanged();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

Q_SIGNALS:
    void before_destroy();

private Q_SLOTS:
    void handleUnregisterShortcut(WSocket* sessionSocket, const QString& name);
    void handleBindKeySequence(WSocket* sessionSocket,
                               const QString& name,
                               const QKeySequence& keySequence,
                               uint mode,
                               uint action);
    void handleBindSwipeGesture(WSocket* sessionSocket, const QString& name, uint finger, uint direction, uint action);
    void handleBindHoldGesture(WSocket* sessionSocket, const QString& name, uint finger, uint action);
    void handleCommit(WSocket* sessionSocket);

private:
    QScopedPointer<ShortcutManagerV2Private> d_ptr;
};
