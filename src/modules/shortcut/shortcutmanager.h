// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include "modules/shortcut/impl/shortcut_manager_impl.h"

#include <QObject>
#include <QQmlEngine>

class ShortcutController;
class ShortcutManagerV2Private;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

enum class ShortcutAction {
    Notify = 1,
    Workspace1 = 2,
    Workspace2 = 3,
    Workspace3 = 4,
    Workspace4 = 5,
    Workspace5 = 6,
    Workspace6 = 7,
    PrevWorkspace = 8,
    NextWorkspace = 9,
    ShowDesktop = 10,
    Maximize = 11,
    CancelMaximize = 12,
    MoveWindow = 13,
    CloseWindow = 14,
    ShowWindowMenu = 15,
    ToggleMultitaskView = 16,
    ToggleFpsDisplay = 17,
    Lockscreen = 18,
    ShutdownMenu = 19,
    Quit = 20,
    TaskSwitchNext = 21,
    TaskSwitchPrev = 22,
    TaskSwitchQuickAdvance = 23,
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
