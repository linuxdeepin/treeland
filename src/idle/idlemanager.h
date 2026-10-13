// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wpointer.h>
#include <wscoplistener.h>

#include <QObject>

struct wlr_idle_notifier_v1;
struct wlr_idle_inhibit_manager_v1;
struct wlr_idle_inhibitor_v1;
class ScreensaverInterfaceV2;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class IdleManager : public QObject, public WObject
{
    Q_OBJECT
public:
    explicit IdleManager(QObject *parent = nullptr);
    ~IdleManager() override;

    void initialize(WServer *server);
    void setScreensaver(ScreensaverInterfaceV2 *screensaver);
    void notifyActivity(WSeat *seat);
    void updateIdleInhibitor();

private:
    void onNewIdleInhibitor(wlr_idle_inhibitor_v1 *inhibitor);

private:
    ScreensaverInterfaceV2 *m_screensaver = nullptr;
    wlr_idle_notifier_v1 *m_idleNotifier = nullptr;
    WPointer<wlr_idle_inhibit_manager_v1> m_idleInhibitManager;
    struct IdleInhibitorEntry {
        wlr_idle_inhibitor_v1 *inhibitor = nullptr;
        WScopedListener destroyListener;
        QList<QMetaObject::Connection> connections;
    };
    std::vector<IdleInhibitorEntry> idleInhibitorEntries;
};
