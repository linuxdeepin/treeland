// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>
#include <QQmlEngine>

class ShortcutController;
class ShortcutManagerV2Private;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSocket;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

// Forward declaration - values defined in treeland-shortcut-manager-v2 protocol
enum class ShortcutAction : uint32_t {
    Notify                = 1,
    Workspace1            = 2,
    Workspace2            = 3,
    Workspace3            = 4,
    Workspace4            = 5,
    Workspace5            = 6,
    Workspace6            = 7,
    PrevWorkspace         = 8,
    NextWorkspace         = 9,
    ShowDesktop           = 10,
    Maximize              = 11,
    CancelMaximize        = 12,
    MoveWindow            = 13,
    CloseWindow           = 14,
    ShowWindowMenu        = 15,
    OpenMultiTaskView     = 16,
    CloseMultiTaskView    = 17,
    ToggleMultitaskView   = 18,
    ToggleFpsDisplay      = 19,
    Lockscreen            = 20,
    ShutdownMenu          = 21,
    Quit                  = 22,
    TaskSwitchEnter       = 23,
    TaskSwitchNext        = 24,
    TaskSwitchPrev        = 25,
    TaskSwitchSameAppNext = 26,
    TaskSwitchSameAppPrev = 27,
};

class ShortcutManagerV2
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT

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

private:
    std::unique_ptr<ShortcutManagerV2Private> d;
};
