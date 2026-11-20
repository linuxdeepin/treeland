// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include "core/waylandserver.h"
#include "wallpaper/wallpapermanager.h"

#include <QHash>
#include <QObject>

class LockScreen;
class LockScreenInterface;
class ILockScreen;
class GreeterProxy;
class QTimer;
class Session;

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
    void showShutdownMenu(WaylandServer::ShutdownMenuFocus focus =
                              WaylandServer::ShutdownMenuFocus::None);
    void showSwitchUser();
    void collectGreeterWallpapers(WallpaperManager::WallpaperLibrary &wallpapers) const;
    void updateGreeterWallpapers(const QObject *item, const QString &source,
                                 TreelandWallpaperInterfaceV1::WallpaperType type = TreelandWallpaperInterfaceV1::Image);

Q_SIGNALS:
    // Emitted when the LockScreen implementation becomes available or unavailable.
    void lockScreenAvailableChanged();
    void sessionLockStateRequested(bool locked);
    void sessionRelockRequested(const QString &id);

private Q_SLOTS:
    // The lock screen interface asks for the menu without a focus preference.
    void onLockScreenShutdown();

private:
    void onSessionLock(Session *session);
    void onSessionUnlock(Session *session);
    void requestSwitchToGreeter();
    void onExtSessionLock(WSessionLock *lock);

private:
    WaylandServer *m_server = nullptr;
    GreeterProxy *m_greeterProxy = nullptr;
    LockScreen *m_lockScreen = nullptr;
    QHash<const QObject *, WallpaperManager::WallpaperLibrary> m_greeterWallpapers;
#ifdef EXT_SESSION_LOCK_V1
    QTimer *m_lockScreenGraceTimer = nullptr;
#endif
};
