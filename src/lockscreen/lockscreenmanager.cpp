// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "lockscreenmanager.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "lockscreen.h"
#include "greeter/greeterproxy.h"
#include "interfaces/lockscreeninterface.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "utils/cmdline.h"
#include "common/treelandlogging.h"

#include <wsessionlock.h>
#include <wsessionlockmanager.h>
#include <wserver.h>

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusInterface>
#include <QThreadPool>
#include <QTimer>

#include <utility>

LockScreenManager::LockScreenManager(WaylandServer *server)
    : QObject(server),
    m_server(server)
{
#ifdef EXT_SESSION_LOCK_V1
    m_lockScreenGraceTimer = new QTimer(this);
    m_lockScreenGraceTimer->setInterval(300);
    m_lockScreenGraceTimer->setSingleShot(true);
#endif
    QDBusConnection::systemBus().connect(
        "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "SessionNew",
        this, SLOT(onSessionNew(QString,QDBusObjectPath)));
}

LockScreenManager::~LockScreenManager() = default;

void LockScreenManager::setGreeterProxy(GreeterProxy *greeter)
{
    m_greeterProxy = greeter;
}

void LockScreenManager::installSessionLock(WServer *server)
{
#ifdef EXT_SESSION_LOCK_V1
    auto *manager = server->attach<WSessionLockManager>();
    if (!m_lockScreen)
        setLockScreenImpl(nullptr);
    connect(manager, &WSessionLockManager::lockCreated,
            this, &LockScreenManager::onExtSessionLock);
#else
    Q_UNUSED(server)
#endif
}

void LockScreenManager::handleLockScreen(LockScreenInterface *lockScreen)
{
    connect(lockScreen, &LockScreenInterface::shutdown, this, &LockScreenManager::showShutdownMenu);
    connect(lockScreen, &LockScreenInterface::lock, this, [this]() {
        if (m_server->isNormalOrMultitaskview())
            showLockScreen(false);
    });
    connect(lockScreen, &LockScreenInterface::switchUser, this, &LockScreenManager::showSwitchUser);
}

void LockScreenManager::onSessionNew(const QString &sessionId, const QDBusObjectPath &sessionPath)
{
    const auto path = sessionPath.path();
    qCDebug(lcTlCore) << "Session new, sessionId:" << sessionId << ", sessionPath:" << path;
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Lock", this, SLOT(onSessionLock()));
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Unlock", this, SLOT(onSessionUnlock()));
}

void LockScreenManager::onSessionLock()
{
    showLockScreen();
}

void LockScreenManager::onSessionUnlock()
{
    if (m_lockScreen) {
        m_lockScreen->unlock();
    }
}

void LockScreenManager::onExtSessionLock(WSessionLock *lock)
{
#ifdef EXT_SESSION_LOCK_V1
    if (m_lockScreen->isLocked()) {
        lock->finish();
        return;
    }

    m_lockScreen->onExternalLock(lock);

    m_server->prepareLockScreenTransition();

    QObject::connect(lock, &WSessionLock::abandoned, this, [this]() {
        m_lockScreenGraceTimer->stop();
        m_server->setNoAnimation(false);
    });

    QObject::connect(lock, &WSessionLock::canceled, this, [this]() {
        m_lockScreenGraceTimer->stop();
    });

    m_lockScreenGraceTimer->disconnect();
    // grace 300ms for possible client to
    connect(m_lockScreenGraceTimer, &QTimer::timeout, this, [this, lock]() {
        m_server->setNoAnimation(true);
        lock->lock();
    });
    m_lockScreenGraceTimer->start();
#endif
}

void LockScreenManager::setLockScreenImpl(ILockScreen *impl)
{
#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
    if (!impl) {
        if (m_lockScreen) {
            delete m_lockScreen;
            m_lockScreen = nullptr;
        }
        return;
    }

    m_lockScreen = new LockScreen(impl, m_server->rootContainer(), m_greeterProxy);
    m_lockScreen->setZ(RootSurfaceContainer::LockScreenZOrder);
    m_lockScreen->setObjectName(QStringLiteral("LockScreenContainer"));
    m_lockScreen->setVisible(false);

    m_greeterProxy->setLockScreen(m_lockScreen);

    for (auto *output : std::as_const(m_server->rootContainer()->outputs())) {
        m_lockScreen->addOutput(output);
    }

    connect(m_lockScreen, &LockScreen::unlock,
            m_server, &WaylandServer::finishLockScreenTransition);
    if (!impl) {
        return;
    }
    if (CmdLine::ref().useLockScreen()) {
        // Start in the undecided state: make the lock screen surface (wallpaper)
        // visible but keep the login UI hidden until DDM decides (ShowGreeter /
        // UserActivateMessage) or the fallback timeout in GreeterProxy fires.
        m_lockScreen->setVisible(true);
    }
#else
    Q_UNUSED(impl)
#endif
}

void LockScreenManager::showLockScreen(bool switchToGreeter)
{
    if (!isLockScreenAvailable()) {
        return;
    }
    // LockScreen::isLocked() is isVisible(), which is also true in the
    // undecided state (surface shown, not yet locked), so check the real
    // lock state instead.
    if (m_greeterProxy->isLocked()) {
        return;
    }

    m_server->prepareLockScreenTransition();
    m_lockScreen->lock();

    if (switchToGreeter) {
        QThreadPool::globalInstance()->start([]() {
            QDBusInterface interface("org.freedesktop.DisplayManager",
                                     "/org/freedesktop/DisplayManager/Seat0",
                                     "org.freedesktop.DisplayManager.Seat",
                                     QDBusConnection::systemBus());
            interface.call("SwitchToGreeter");
        });
    }
}

bool LockScreenManager::isLockScreenAvailable() const
{
    return m_lockScreen && m_lockScreen->available();
}

void LockScreenManager::showShutdownMenu()
{
    if (!isLockScreenAvailable() || !m_server->isNormalOrMultitaskview()) {
        return;
    }

    m_server->prepareLockScreenTransition(true);
    m_lockScreen->shutdown();
}

void LockScreenManager::showSwitchUser()
{
    if (!isLockScreenAvailable() || !m_server->isNormalOrMultitaskview()) {
        return;
    }

    m_server->prepareLockScreenTransition();
    m_lockScreen->switchUser();
}
