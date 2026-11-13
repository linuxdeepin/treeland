// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "greeterproxy.h"
#include "session/sessionmanager.h"

// Treeland
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "core/waylandserver.h"
#include "session/session.h"
#include "common/treelandlogging.h"
#include "utils/cmdline.h"
#include "lockscreen/lockscreen.h"
#include "lockscreen/lockscreenmanager.h"

// DDM
#include <Login1Manager.h>
#include <Messages.h>
#include <SocketWriter.h>

// Qt
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QLocalSocket>
#include <QScopeGuard>
#include <QVariantMap>

// Waylib
#include <woutputrenderwindow.h>

// System
#include <security/pam_appl.h>
#include <systemd/sd-login.h>
#include <pwd.h>

#include <QTimer>

using namespace DDM;

/////////////////////
// Local Functions //
/////////////////////

static bool localValidation(const QString &user, const QString &password)
{
    auto utf8Password = password.toUtf8();
    struct pam_conv conv = {
        []([[maybe_unused]] int num_msg,
           [[maybe_unused]] const struct pam_message **msg,
           struct pam_response **resp,
           void *appdata_ptr) {
            // pam uses free, we must malloc
            auto *reply = static_cast<pam_response *>(malloc(sizeof(pam_response)));
            reply->resp = strdup(static_cast<const char *>(appdata_ptr)); // Send password to PAM
            reply->resp_retcode = 0;
            *resp = reply;
            return PAM_SUCCESS;
        },
        static_cast<void *>(utf8Password.data()),
    };

    pam_handle_t *pamh = nullptr;

    int retval = pam_start("login", user.toUtf8().data(), &conv, &pamh);
    if (retval != PAM_SUCCESS) {
        return false;
    }

    retval = pam_authenticate(pamh, 0);
    pam_end(pamh, retval);

    return retval == PAM_SUCCESS;
}

static bool isUserDesktopSession(const QString &id)
{
    const auto encodedId = id.toLocal8Bit();
    char *type = nullptr, *sessionClass = nullptr;
    const auto cleanup = qScopeGuard([&] {
        free(type);
        free(sessionClass);
    });
    // An account's TTY/SSH or greeter session must not control its desktop lock.
    if (sd_session_get_type(encodedId.constData(), &type) < 0
        || sd_session_get_class(encodedId.constData(), &sessionClass) < 0
        || sd_session_is_remote(encodedId.constData()) != 0
        || QString::fromLocal8Bit(type) != QStringLiteral("wayland")
        || (QString::fromLocal8Bit(sessionClass) != QStringLiteral("user")
            && QString::fromLocal8Bit(sessionClass) != QStringLiteral("user-early")))
        return false;
    return true;
}

static inline UserModel *userModel()
{
    return WaylandServer::instance()->userModel();
}

static inline SessionModel *sessionModel()
{
    return WaylandServer::instance()->sessionModel();
}

/////////////////////////////////
// Constructor & Deconstructor //
/////////////////////////////////

GreeterProxy::GreeterProxy(QObject *parent)
    : QObject(parent)
{
    m_socket = new QLocalSocket(this);
    auto conn = QDBusConnection::systemBus();
    auto *displayManagerWatcher =
        new QDBusServiceWatcher(QStringLiteral("org.deepin.DisplayManager"),
                                conn,
                                QDBusServiceWatcher::WatchForRegistration
                                    | QDBusServiceWatcher::WatchForUnregistration,
                                this);
    connect(displayManagerWatcher,
            &QDBusServiceWatcher::serviceRegistered,
            this,
            [this](const QString &) {
                qCInfo(lcTlGreeter) << "Display manager service registered";
                updateAuthSocket();
            });
    connect(displayManagerWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            [this](const QString &) {
                qCWarning(lcTlGreeter) << "Display manager service unregistered";
                m_socket->abort();
            });

    // connect signals
    connect(m_socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &GreeterProxy::error);

    // Fallback: if DDM never decides (ShowGreeter / UserActivateMessage), show
    // the greeter after a grace period instead of staying on the wallpaper.
    m_undecidedTimer = new QTimer(this);
    m_undecidedTimer->setSingleShot(true);
    m_undecidedTimer->setInterval(5000);
    connect(m_undecidedTimer, &QTimer::timeout, this, [this] {
        if (m_undecided) {
            qCWarning(lcTlGreeter) << "DDM did not decide in time, showing greeter by fallback";
            lock();
        }
    });
    m_undecided = CmdLine::ref().useLockScreen();

    conn.connect(Logind::serviceName(),
                 Logind::managerPath(),
                 Logind::managerIfaceName(),
                 "SessionNew",
                 this,
                 SLOT(onSessionNew(QString, QDBusObjectPath)));
    conn.connect(Logind::serviceName(),
                 Logind::managerPath(),
                 Logind::managerIfaceName(),
                 "SessionRemoved",
                 this,
                 SLOT(onSessionRemoved(QString, QDBusObjectPath)));
    conn.connect("org.deepin.DisplayManager",
                 "/org/deepin/DisplayManager",
                 "org.deepin.DisplayManager",
                 "AuthInfoChanged",
                 this,
                 SLOT(updateAuthSocket()));

    updateAuthSocket();
}

GreeterProxy::~GreeterProxy() { }

////////////////////////
// Properties setters //
////////////////////////

void GreeterProxy::setShowShutdownView(bool show) {
    if (m_showShutdownView != show) {
        m_showShutdownView = show;
        Q_EMIT showShutdownViewChanged(show);
    }
}

void GreeterProxy::bindLockScreenManager(LockScreenManager *manager)
{
    connect(manager, &LockScreenManager::sessionLockStateRequested,
            this, &GreeterProxy::setLock, Qt::UniqueConnection);
    connect(manager, &LockScreenManager::sessionRelockRequested,
            this, &GreeterProxy::onSessionRelockRequested, Qt::UniqueConnection);
}

void GreeterProxy::setLock(bool isLocked)
{
    if (isLocked && !m_isLocked) {
        m_isLocked = true;
        if (m_undecided) {
            m_undecided = false;
            m_undecidedTimer->stop();
            Q_EMIT undecidedChanged(false);
        }
        if (m_lockScreen && !m_lockScreen->isVisible())
            m_lockScreen->lock();
        Q_EMIT lockChanged(true);
    } else if (!isLocked && m_isLocked) {
        m_failedAttempts = 0;
        Q_EMIT failedAttemptsChanged(0);
        m_isLocked = false;
        if (m_lockScreen && m_lockScreen->isVisible())
            Q_EMIT m_lockScreen->unlock();
        Q_EMIT lockChanged(false);
    }
    if (m_showShutdownView)
        setShowShutdownView(false);
}

///////////
// Slots //
///////////

void GreeterProxy::powerOff()
{
    SocketWriter(m_socket) << quint32(GreeterMessages::PowerOff);
}

void GreeterProxy::reboot()
{
    SocketWriter(m_socket) << quint32(GreeterMessages::Reboot);
}

void GreeterProxy::suspend()
{
    SocketWriter(m_socket) << quint32(GreeterMessages::Suspend);
}

void GreeterProxy::hibernate()
{
    SocketWriter(m_socket) << quint32(GreeterMessages::Hibernate);
}

void GreeterProxy::hybridSleep()
{
    SocketWriter(m_socket) << quint32(GreeterMessages::HybridSleep);
}

void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex)
{
    if (!m_socket->isValid()) {
        qCDebug(lcTlGreeter) << "Socket is not valid. Local password check.";
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
        return;
    }

    // get model index
    QModelIndex index = sessionModel()->index(sessionIndex, 0);

    // send command to the daemon
    QString name = sessionModel()->data(index, SessionModel::FileRole).toString();
    qCInfo(lcTlGreeter) << "Logging user" << user << "in with session" << name;
    SocketWriter(m_socket) << quint32(GreeterMessages::Login) << user << password << name;
}

void GreeterProxy::unlock(const QString &user, const QString &password)
{
    if (!m_socket->isValid()) {
        qCDebug(lcTlGreeter) << "Socket is not valid. Local password check.";
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
        return;
    }

    auto userInfo = userModel()->get(user);
    if (userInfo.isValid()) {
        qCInfo(lcTlGreeter) << "Unlocking user" << user;
        SocketWriter(m_socket) << quint32(GreeterMessages::Unlock) << user << password;
    }
}

void GreeterProxy::logout()
{
    auto session = WaylandServer::instance()->sessionManager()->activeSession().lock();
    qCInfo(lcTlGreeter) << "Logging user" << session->username() << "out with session id" << session->id();
    SocketWriter(m_socket) << quint32(GreeterMessages::Logout) << session->id();
}

void GreeterProxy::lock()
{
    auto session = WaylandServer::instance()->sessionManager()->activeSession().lock();
    if (!session || session->username() == "dde") {
        qCInfo(lcTlGreeter) << "Trying to lock when no user session active, show lockscreen directly.";
        setLock(true);
        return;
    }
    qCInfo(lcTlGreeter) << "Locking user" << session->username() << "with session id" << session->id();
    SocketWriter(m_socket) << quint32(GreeterMessages::Lock) << session->id();
}

void GreeterProxy::onSessionRelockRequested(const QString &id)
{
    SocketWriter(m_socket) << quint32(GreeterMessages::Lock) << id;
}

//////////////////////////////
// Logind session listeners //
//////////////////////////////

void GreeterProxy::onSessionNew(const QString &id, const QDBusObjectPath &path)
{
    QByteArray sessionBa = id.toLocal8Bit();
    const char *session = sessionBa.constData();
    char *username = nullptr;
    char *service = nullptr;
    auto guard = qScopeGuard([&]() {
        if (username)
            free(username);
        if (service)
            free(service);
    });
    if (sd_session_get_username(session, &username) < 0) {
        qCWarning(lcTlGreeter) << "sd_session_get_username() failed for session id:" << id;
        return;
    }
    if (sd_session_get_service(session, &service) < 0) {
        qCWarning(lcTlGreeter) << "sd_session_get_service() failed for session id:" << id;
        return;
    }

    if (strncmp(service, "ddm", 3) == 0 && isUserDesktopSession(id)) {
        QString user = QString::fromLocal8Bit(username);
        qCInfo(lcTlGreeter) << "New session added: id=" << id << ", user=" << user;
        userModel()->updateUserLoginState(user, true);

        // userLoggedIn signal is connected with WaylandServer::updateActiveUserSession
        Q_EMIT userModel()->userLoggedIn(user, id);
        const auto desktop = WaylandServer::instance()->sessionManager()->sessionForId(id);
        if (!desktop || !desktop->subscribeLogind(path.path())) {
            qCWarning(lcTlGreeter) << "Cannot bind logind path to new desktop:" << id << path.path();
            return;
        }

        if (userModel()->currentUserName() == user)
            setLock(false);
        if (!m_hasActiveSession) {
            m_hasActiveSession = true;
            Q_EMIT hasActiveSessionChanged(true);
        }
    }
}

void GreeterProxy::restoreLogindPath(const QString &id)
{
    const auto session = WaylandServer::instance()->sessionManager()->sessionForId(id);
    if (!session || !session->logindPath().isEmpty())
        return;
    auto request = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("GetSession"));
    request << id;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, expected = std::weak_ptr<Session>(session)](QDBusPendingCallWatcher *reply) {
                onLogindPathRestored(reply, expected);
            });
}

void GreeterProxy::onLogindPathRestored(QDBusPendingCallWatcher *watcher,
                                       const std::weak_ptr<Session> &expected)
{
    const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
    watcher->deleteLater();
    const auto session = expected.lock();
    if (!session || WaylandServer::instance()->sessionManager()->sessionForId(session->id()) != session
        || !session->logindPath().isEmpty())
        return;
    if (reply.isError()) {
        qCWarning(lcTlGreeter) << "Cannot restore logind path:" << session->id() << reply.error().message();
        return;
    }
    if (isUserDesktopSession(session->id())
        && !session->subscribeLogind(reply.value().path()))
        qCWarning(lcTlGreeter) << "Cannot bind restored logind path:" << session->id() << reply.value().path();
}

void GreeterProxy::onSessionRemoved(const QString &id, const QDBusObjectPath &path)
{
    auto session = WaylandServer::instance()->sessionManager()->sessionForId(id);
    if (session && (session->logindPath().isEmpty() || session->logindPath() == path.path())) {
        QString username = session->username();
        qCInfo(lcTlGreeter) << "Session removed: id=" << id << ", user=" << username;
        if (WaylandServer::instance()->sessionManager()->activeSession().lock() == session)
            setLock(true);
        userModel()->updateUserLoginState(username, false);
        WaylandServer::instance()->sessionManager()->removeSession(session);
    }

    if (m_hasActiveSession && WaylandServer::instance()->sessionManager()->sessions().isEmpty()) {
        m_hasActiveSession = false;
        Q_EMIT hasActiveSessionChanged(false);
    }
}

///////////////////////
// DDM Communication //
///////////////////////

bool GreeterProxy::isConnected() const
{
    return m_socket->state() == QLocalSocket::ConnectedState;
}

void GreeterProxy::connected()
{
    qCInfo(lcTlGreeter) << "Connected to the ddm";

    sendConnect();
}

void GreeterProxy::enableHandshake()
{
    if (m_handshakeEnabled) {
        return;
    }

    m_handshakeEnabled = true;
    if (m_undecided) {
        m_undecidedTimer->start();
    }
    sendConnect();
}

void GreeterProxy::sendConnect()
{
    if (!m_handshakeEnabled || !isConnected()) {
        return;
    }

    SocketWriter(m_socket)
        << quint32(GreeterMessages::Connect)
        << WaylandServer::instance()->sessionManager()->globalSession()->socket()->fullServerName();
}

void GreeterProxy::disconnected()
{
    qCWarning(lcTlGreeter) << "Disconnected from the ddm";

    Q_EMIT socketDisconnected();
}

void GreeterProxy::error()
{
    qCCritical(lcTlGreeter) << "Socket error: " << m_socket->errorString();
}

void GreeterProxy::readyRead()
{
    // input stream
    QDataStream input(m_socket);

    while (input.device()->bytesAvailable()) {
        input.startTransaction();

        // read message
        quint32 message = 0;
        input >> message;

        switch (DaemonMessages(message)) {
        case DaemonMessages::Capabilities: {
            // log message
            qCDebug(lcTlGreeter) << "Message received from daemon: Capabilities";

            // read capabilities
            quint32 capabilities = 0;
            input >> capabilities;
            if (!input.commitTransaction())
                return;

            // parse capabilities
            m_canPowerOff = capabilities & Capability::PowerOff;
            m_canReboot = capabilities & Capability::Reboot;
            m_canSuspend = capabilities & Capability::Suspend;
            m_canHibernate = capabilities & Capability::Hibernate;
            m_canHybridSleep = capabilities & Capability::HybridSleep;

            // Q_EMIT signals
            Q_EMIT canPowerOffChanged(m_canPowerOff);
            Q_EMIT canRebootChanged(m_canReboot);
            Q_EMIT canSuspendChanged(m_canSuspend);
            Q_EMIT canHibernateChanged(m_canHibernate);
            Q_EMIT canHybridSleepChanged(m_canHybridSleep);
        } break;
        case DaemonMessages::HostName: {
            qCDebug(lcTlGreeter) << "Message received from daemon: HostName";

            // read host name
            input >> m_hostName;
            if (!input.commitTransaction())
                return;

            // Q_EMIT signal
            Q_EMIT hostNameChanged(m_hostName);
        } break;
        case DaemonMessages::LastLogin: {
            qCDebug(lcTlGreeter) << "Message received from daemon: LastLogin";

            QString lastUser, lastSession;
            input >> lastUser >> lastSession;
            if (!input.commitTransaction())
                return;

            userModel()->setLastUser(lastUser);
            sessionModel()->setLastSession(lastSession);
            Q_EMIT initialUserSelected();
        } break;
        case DaemonMessages::SessionDirs: {
            qCDebug(lcTlGreeter) << "Message received from daemon: SessionDirs";

            // read the directories the sessions are installed in
            QStringList waylandSessionDirs, x11SessionDirs;
            input >> waylandSessionDirs >> x11SessionDirs;
            if (!input.commitTransaction())
                return;

            sessionModel()->setSessionDirs(waylandSessionDirs, x11SessionDirs);
        } break;
        case DaemonMessages::LoginFailed: {
            QString user;
            input >> user;
            if (!input.commitTransaction())
                return;

            qCDebug(lcTlGreeter) << "Message received from daemon: LoginFailed" << user;

            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        } break;
        case DaemonMessages::InformationMessage: {
            QString message;
            input >> message;
            if (!input.commitTransaction())
                return;

            qCDebug(lcTlGreeter) << "Information Message received from daemon: " << message;
            Q_EMIT informationMessage(message);
        } break;
        case DaemonMessages::SwitchToGreeter: {
            if (!input.commitTransaction())
                return;

            qCInfo(lcTlGreeter) << "switch to greeter";
            lock();
        } break;
#ifndef DDM_VERSION_LT_0_3_8
        case DaemonMessages::ShowGreeter: {
            if (!input.commitTransaction())
                return;

            qCInfo(lcTlGreeter) << "show greeter";
            // Full lock screen transition (LockScreen mode, hidden workspace),
            // matching the previous startup behavior of showLockScreen().
            WaylandServer::instance()->showLockScreen(false);
        } break;
#endif
        case DaemonMessages::UserActivateMessage: {
            QString user;
            QString sessionId;
            input >> user >> sessionId;
            if (!input.commitTransaction())
                return;

            // NOTE: maybe DDM will active dde user.
            if (!userModel()->getUser(user)) {
                qCInfo(lcTlGreeter) << "activate user, but switch to greeter";
                lock();
                break;
            }

            if (!WaylandServer::instance()->activateUserSession(user, sessionId)) {
                qCWarning(lcTlGreeter) << "Failed to activate desktop for user" << user << "session" << sessionId;
                break;
            }

            restoreLogindPath(sessionId);
            qCInfo(lcTlGreeter) << "activate successfully: " << user << ", XDG_SESSION_ID: " << sessionId;

            setLock(false);

            // DDM drives the direct login (autologin / unlock), so the greeter
            // must hide unconditionally. onSessionNew's setLock(false) may never
            // fire (currentUserName mismatch / session service not "ddm"). When
            // still in the undecided state, the lock screen surface is visible
            // (wallpaper) but not locked, so setLock(false) alone would no-op:
            // restore the desktop mode and hide the surface explicitly.
            if (m_undecided) {
                m_undecided = false;
                m_undecidedTimer->stop();
                Q_EMIT undecidedChanged(false);
                if (m_lockScreen && m_lockScreen->isVisible()) {
                    Q_EMIT m_lockScreen->unlock();
                    m_lockScreen->setVisible(false);
                }
            }
        } break;
        case DaemonMessages::UserLoggedIn: {
            QString user;
            QString sessionId;
            input >> user >> sessionId;
            if (!input.commitTransaction())
                return;

            // This will happen after a crash recovery of treeland
            qCInfo(lcTlGreeter) << "User " << user << " is already logged in";
            auto userPtr = userModel()->getUser(user);

            // TODO: persist users entered via "Other" so they survive in UserModel; then remove
            // this fallback
            if (!userPtr && userModel()->tryAddNssUser(user)) {
                userPtr = userModel()->getUser(user);
            } else if (!userPtr) {
                // account already listed under another name form (user@domain); match by uid
                if (const passwd *pw = ::getpwnam(user.toLocal8Bit().constData())) {
                    userPtr = userModel()->getUser(static_cast<uid_t>(pw->pw_uid));
                }
            }

            if (userPtr) {
                userModel()->updateUserLoginState(user, true);
                Q_EMIT userModel()->userLoggedIn(user, sessionId);
                restoreLogindPath(sessionId);
            } else {
                qCWarning(lcTlGreeter) << "User " << user << " logged in but not found";
            }
        } break;
        default: {
            if (!input.commitTransaction())
                return;
            qCWarning(lcTlGreeter) << "Unknown message received from daemon." << message;
            return;
        }
        }
    }
}

void GreeterProxy::updateAuthSocket()
{
    QThreadPool::globalInstance()->start([this]() {
        QDBusInterface manager("org.deepin.DisplayManager",
                               "/org/deepin/DisplayManager",
                               "org.deepin.DisplayManager",
                               QDBusConnection::systemBus());
        QDBusReply<QString> reply = manager.call("AuthInfo");
        if (!reply.isValid()) {
            qCWarning(lcTlGreeter) << "Failed to get auth info from display manager:" << reply.error().message();
            return;
        }
        const QString &socket = reply.value();
        QMetaObject::invokeMethod(this, [this, socket] {
            if (m_socket->state() == QLocalSocket::ConnectedState)
                m_socket->disconnectFromServer();

            m_socket->connectToServer(socket);
        });
    });
}
