// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "greeterproxy.h"

// Treeland
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "seat/helper.h"
#include "session/session.h"
#include "common/treelandlogging.h"
#include "core/lockscreen.h"

// DDM
#include <Login1Manager.h>
#include <Login1Session.h>
#include <Messages.h>
#include <SocketWriter.h>

// Qt
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusReply>
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

static inline UserModel *userModel()
{
    return Helper::instance()->userModel();
}

static inline SessionModel *sessionModel()
{
    return Helper::instance()->sessionModel();
}

/////////////////////////////////
// Constructor & Deconstructor //
/////////////////////////////////

GreeterProxy::GreeterProxy(QObject *parent)
    : QObject(parent)
{
    m_socket = new QLocalSocket(this);

    // connect signals
    connect(m_socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &GreeterProxy::error);

    auto conn = QDBusConnection::systemBus();
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

void GreeterProxy::setLock(bool isLocked)
{
    if (isLocked && !m_isLocked) {
        m_isLocked = true;
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
        qCDebug(treelandGreeter) << "Socket is not valid. Local password check.";
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
    DDM::Session::Type type =
        static_cast<DDM::Session::Type>(sessionModel()->data(index, SessionModel::TypeRole).toInt());
    QString name = sessionModel()->data(index, SessionModel::FileRole).toString();
    qCInfo(treelandGreeter) << "Logging user" << user << "in with" << type << "session" << name;
    DDM::Session session(type, name);
    SocketWriter(m_socket) << quint32(GreeterMessages::Login) << user << password << session;
}

void GreeterProxy::unlock(const QString &user, const QString &password)
{
    if (!m_socket->isValid()) {
        qCDebug(treelandGreeter) << "Socket is not valid. Local password check.";
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
        return;
    }

    auto userInfo = userModel()->get(user);
    if (userInfo.isValid()) {
        qCInfo(treelandGreeter) << "Unlocking user" << user;
        SocketWriter(m_socket) << quint32(GreeterMessages::Unlock) << user << password;
    }
}

void GreeterProxy::logout()
{
    auto session = Helper::instance()->sessionManager()->activeSession().lock();
    qCInfo(treelandGreeter) << "Logging user" << session->username() << "out with session id" << session->id();
    SocketWriter(m_socket) << quint32(GreeterMessages::Logout) << session->id();
}

void GreeterProxy::lock()
{
    auto session = Helper::instance()->sessionManager()->activeSession().lock();
    if (!session || session->username() == "dde") {
        qCInfo(treelandGreeter) << "Trying to lock when no user session active, show lockscreen directly.";
        setLock(true);
        return;
    }
    qCInfo(treelandGreeter) << "Locking user" << session->username() << "with session id" << session->id();
    SocketWriter(m_socket) << quint32(GreeterMessages::Lock) << session->id();
}

//////////////////////////////
// Logind session listeners //
//////////////////////////////

void GreeterProxy::onSessionNew(const QString &id, [[maybe_unused]] const QDBusObjectPath &path)
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
        qCWarning(treelandGreeter) << "sd_session_get_username() failed for session id:" << id;
        return;
    }
    if (sd_session_get_service(session, &service) < 0) {
        qCWarning(treelandGreeter) << "sd_session_get_service() failed for session id:" << id;
        return;
    }

    if (strcmp(service, "ddm") == 0) {
        QString user = QString::fromLocal8Bit(username);
        qCInfo(treelandGreeter) << "New session added: id=" << id << ", user=" << user;
        userModel()->updateUserLoginState(user, true);
        // userLoggedIn signal is connected with Helper::updateActiveUserSession
        Q_EMIT userModel()->userLoggedIn(user, id.toInt());

        if (userModel()->currentUserName() == user)
            setLock(false);
        if (!m_hasActiveSession) {
            m_hasActiveSession = true;
            Q_EMIT hasActiveSessionChanged(true);
        }
    }
}

void GreeterProxy::onSessionRemoved(const QString &id, [[maybe_unused]] const QDBusObjectPath &path)
{
    auto session = Helper::instance()->sessionManager()->sessionForId(id.toInt());
    if (session) {
        QString username = session->username();
        qCInfo(treelandGreeter) << "Session removed: id=" << id << ", user=" << username;
        if (Helper::instance()->sessionManager()->activeSession().lock() == session)
            setLock(true);
        userModel()->updateUserLoginState(username, false);
        Helper::instance()->sessionManager()->removeSession(session);
    }

    if (m_hasActiveSession && Helper::instance()->sessionManager()->sessions().isEmpty()) {
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
    qCInfo(treelandGreeter) << "Connected to the ddm";

    SocketWriter(m_socket)
        << quint32(GreeterMessages::Connect)
        << Helper::instance()->sessionManager()->globalSession()->socket()->fullServerName();
}

void GreeterProxy::disconnected()
{
    qCWarning(treelandGreeter) << "Disconnected from the ddm";

    Q_EMIT socketDisconnected();
}

void GreeterProxy::error()
{
    qCCritical(treelandGreeter) << "Socket error: " << m_socket->errorString();
}

void GreeterProxy::readyRead()
{
    // input stream
    QDataStream input(m_socket);

    while (input.device()->bytesAvailable()) {
        // read message
        quint32 message;
        input >> message;

        switch (DaemonMessages(message)) {
        case DaemonMessages::Capabilities: {
            // log message
            qCDebug(treelandGreeter) << "Message received from daemon: Capabilities";

            // read capabilities
            quint32 capabilities;
            input >> capabilities;

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
            qCDebug(treelandGreeter) << "Message received from daemon: HostName";

            // read host name
            input >> m_hostName;

            // Q_EMIT signal
            Q_EMIT hostNameChanged(m_hostName);
        } break;
        case DaemonMessages::LoginFailed: {
            QString user;
            input >> user;

            qCDebug(treelandGreeter) << "Message received from daemon: LoginFailed" << user;

            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        } break;
        case DaemonMessages::InformationMessage: {
            QString message;
            input >> message;

            qCDebug(treelandGreeter) << "Information Message received from daemon: " << message;
            Q_EMIT informationMessage(message);
        } break;
        case DaemonMessages::SwitchToGreeter: {
            qCInfo(treelandGreeter) << "switch to greeter";
            lock();
        } break;
        case DaemonMessages::UserActivateMessage: {
            QString user;
            int sessionId;
            input >> user >> sessionId;

            // NOTE: maybe DDM will active dde user.
            if (!userModel()->getUser(user)) {
                qCInfo(treelandGreeter) << "activate user, but switch to greeter";
                lock();
                break;
            }

            userModel()->setCurrentUserName(user);

            qCInfo(treelandGreeter) << "activate successfully: " << user << ", XDG_SESSION_ID: " << sessionId;
        } break;
        case DaemonMessages::UserLoggedIn: {
            QString user;
            int sessionId;
            input >> user >> sessionId;

            // This will happen after a crash recovery of treeland
            qCInfo(treelandGreeter) << "User " << user << " is already logged in";
            auto userPtr = userModel()->getUser(user);
            if (userPtr) {
                userModel()->updateUserLoginState(user, true);
                Q_EMIT userModel()->userLoggedIn(user, sessionId);
            } else {
                qCWarning(treelandGreeter) << "User " << user << " logged in but not found";
            }
        } break;
        default: {
            qCWarning(treelandGreeter) << "Unknown message received from daemon." << message;
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
            qCWarning(treelandGreeter) << "Failed to get auth info from display manager:" << reply.error().message();
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
