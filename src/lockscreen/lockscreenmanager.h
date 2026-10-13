// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>

class WaylandServer;
class LockScreen;
class LockScreenInterface;
class ILockScreen;
class GreeterProxy;
class QTimer;
class QDBusObjectPath;
Q_MOC_INCLUDE(<QDBusObjectPath>)

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSessionLock;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class LockScreenManager : public QObject
{
    Q_OBJECT
public:
    explicit LockScreenManager(WaylandServer *server);
    ~LockScreenManager() override;

    void setGreeterProxy(GreeterProxy *greeter);
    void installSessionLock(WServer *server);
    void handleLockScreen(LockScreenInterface *lockScreen);
    void setLockScreenImpl(ILockScreen *impl);
    void showLockScreen(bool switchToGreeter = true);
    bool isLockScreenAvailable() const;
    void showShutdownMenu();
    void showSwitchUser();

private Q_SLOTS:
    void onSessionNew(const QString &sessionId, const QDBusObjectPath &objectPath);
    void onSessionLock();
    void onSessionUnlock();

private:
    void onExtSessionLock(WSessionLock *lock);

private:
    WaylandServer *m_server = nullptr;
    GreeterProxy *m_greeterProxy = nullptr;
    LockScreen *m_lockScreen = nullptr;
#ifdef EXT_SESSION_LOCK_V1
    QTimer *m_lockScreenGraceTimer = nullptr;
#endif
};
