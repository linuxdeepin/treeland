/***************************************************************************
 * Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 ***************************************************************************/

#include "greeterproxy.h"

#include "DDMDisplayManager.h"
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "seat/helper.h"
#include "common/treelandlogging.h"

#include <DisplayManager.h>
#include <DisplayManagerSession.h>
#include <Login1Manager.h>
#include <Login1Session.h>
#include <Messages.h>
#include <SocketWriter.h>
#include <security/pam_appl.h>
#include <pwd.h>

#include <DDBusSender>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLocalSocket>
#include <QVariantMap>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusReply>

#include <woutputrenderwindow.h>

using namespace DDM;

class GreeterProxyPrivate
{
public:
    SessionModel *sessionModel{ nullptr };
    UserModel *userModel{ nullptr };
    QLocalSocket *socket{ nullptr };
    org::deepin::DisplayManager *ddmDisplayManager{ nullptr };
    QDBusUnixFileDescriptor authFd;
    QString hostName;
    bool canPowerOff{ false };
    bool canReboot{ false };
    bool canSuspend{ false };
    bool canHibernate{ false };
    bool canHybridSleep{ false };
    bool isLocked{ false };
    bool isLoggedIn{ false };
};

GreeterProxy::GreeterProxy(QObject *parent)
    : QObject(parent)
    , d(new GreeterProxyPrivate())
{
    qDBusRegisterMetaType<SessionInfo>();
    qDBusRegisterMetaType<QList<SessionInfo>>();

    d->socket = new QLocalSocket(this);

    // connect signals
    connect(d->socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
    connect(d->socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
    connect(d->socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
    connect(d->socket, &QLocalSocket::errorOccurred, this, &GreeterProxy::error);

    connect(this, &GreeterProxy::loginSucceeded, this, [this]([[maybe_unused]] QString user) {
        d->isLoggedIn = true;
        Q_EMIT isLoggedInChanged();
    });

    d->ddmDisplayManager = new org::deepin::DisplayManager("org.deepin.DisplayManager",
                                                           "/org/deepin/DisplayManager",
                                                           QDBusConnection::systemBus());
    connect(d->ddmDisplayManager,
            &org::deepin::DisplayManager::AuthInfoChanged,
            this,
            &GreeterProxy::updateAuthSocket);

    updateAuthSocket();
}

GreeterProxy::~GreeterProxy()
{
    delete d;
}

const QString &GreeterProxy::hostName() const
{
    return d->hostName;
}

SessionModel *GreeterProxy::sessionModel() const
{
    return d->sessionModel;
}

void GreeterProxy::setSessionModel(SessionModel *model)
{
    d->sessionModel = model;
    Q_EMIT sessionModelChanged(model);
}

UserModel *GreeterProxy::userModel() const
{
    return d->userModel;
}

void GreeterProxy::setUserModel(UserModel *model)
{
    d->userModel = model;
    Q_EMIT userModelChanged(model);
}

bool GreeterProxy::isLocked() const
{
    return d->isLocked;
}

bool GreeterProxy::isLoggedIn() const
{
    return d->isLoggedIn;
}

bool GreeterProxy::canPowerOff() const
{
    return d->canPowerOff;
}

bool GreeterProxy::canReboot() const
{
    return d->canReboot;
}

bool GreeterProxy::canSuspend() const
{
    return d->canSuspend;
}

bool GreeterProxy::canHibernate() const
{
    return d->canHibernate;
}

bool GreeterProxy::canHybridSleep() const
{
    return d->canHybridSleep;
}

bool GreeterProxy::isConnected() const
{
    return d->socket->state() == QLocalSocket::ConnectedState;
}

void GreeterProxy::powerOff()
{
    SocketWriter(d->socket) << quint32(GreeterMessages::PowerOff);
}

void GreeterProxy::reboot()
{
    SocketWriter(d->socket) << quint32(GreeterMessages::Reboot);
}

void GreeterProxy::suspend()
{
    SocketWriter(d->socket) << quint32(GreeterMessages::Suspend);
}

void GreeterProxy::hibernate()
{
    SocketWriter(d->socket) << quint32(GreeterMessages::Hibernate);
}

void GreeterProxy::hybridSleep()
{
    SocketWriter(d->socket) << quint32(GreeterMessages::HybridSleep);
}

void GreeterProxy::init()
{
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
}

void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex)
{
    if (!d->socket->isValid()) {
        qCDebug(treelandGreeter) << "Socket is not valid. Local password check.";
        if (localValidation(user, password)) {
            Q_EMIT loginSucceeded(user);
        } else {
            Q_EMIT loginFailed(user);
        }
        return;
    }

    if (!d->sessionModel) {
        qCCritical(treelandGreeter) << "Session model is not set.";
        return;
    }

    // get model index
    QModelIndex index = d->sessionModel->index(sessionIndex, 0);

    // send command to the daemon
    DDM::Session::Type type =
        static_cast<DDM::Session::Type>(d->sessionModel->data(index, SessionModel::TypeRole).toInt());
    QString name = d->sessionModel->data(index, SessionModel::FileRole).toString();
    DDM::Session session(type, name);
    SocketWriter(d->socket) << quint32(GreeterMessages::Login) << user << password << session;
}

void GreeterProxy::unlock(const QString &user, const QString &password)
{
    if (!d->socket->isValid()) {
        qCDebug(treelandGreeter) << "Socket is not valid. Local password check.";
        if (localValidation(user, password)) {
            Q_EMIT loginSucceeded(user);
        } else {
            Q_EMIT loginFailed(user);
        }
        return;
    }

    auto userInfo = userModel()->get(user);
    if (userInfo.isValid()) {
        SocketWriter(d->socket) << quint32(GreeterMessages::Unlock) << user << password;
    }
}

void GreeterProxy::logout()
{
    qCDebug(treelandGreeter) << "Logout.";
    auto session = Helper::instance()->activeSession().lock();
    SocketWriter(d->socket) << quint32(GreeterMessages::Logout) << session->id;
}

void GreeterProxy::connected()
{
    qCDebug(treelandGreeter) << "Connected to the daemon.";

    SocketWriter(d->socket) << quint32(GreeterMessages::Connect)
                            << Helper::instance()->globalWaylandSocket()->fullServerName();
}

void GreeterProxy::disconnected()
{
    qCDebug(treelandGreeter) << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();
}

void GreeterProxy::error()
{
    qCCritical(treelandGreeter) << "Socket error: " << d->socket->errorString();
}

void GreeterProxy::onSessionNew(const QString &id, const QDBusObjectPath &path)
{
    QThreadPool::globalInstance()->start([this, id, path] {
        OrgFreedesktopLogin1SessionInterface session(Logind::serviceName(),
                                                     path.path(),
                                                     QDBusConnection::systemBus());
        QString username = session.name();
        QString service = session.service();
        if (service == QStringLiteral("ddm")) {
            QMetaObject::invokeMethod(this, [this, username, id]() {
                userModel()->updateUserLoginState(username, true);
                // userLoggedIn signal is connected with Helper::updateActiveUserSession
                Q_EMIT d->userModel->userLoggedIn(username, id.toInt());
                updateLocketState();
            });
        }
    });
}

void GreeterProxy::onSessionRemoved(const QString &id, [[maybe_unused]] const QDBusObjectPath &path)
{
    auto session = Helper::instance()->sessionForId(id.toInt());
    if (session) {
        userModel()->updateUserLoginState(session->username, false);
        updateLocketState();
        Helper::instance()->removeSession(session);
    }
}

void GreeterProxy::readyRead()
{
    // input stream
    QDataStream input(d->socket);

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
            d->canPowerOff = capabilities & Capability::PowerOff;
            d->canReboot = capabilities & Capability::Reboot;
            d->canSuspend = capabilities & Capability::Suspend;
            d->canHibernate = capabilities & Capability::Hibernate;
            d->canHybridSleep = capabilities & Capability::HybridSleep;

            // Q_EMIT signals
            Q_EMIT canPowerOffChanged(d->canPowerOff);
            Q_EMIT canRebootChanged(d->canReboot);
            Q_EMIT canSuspendChanged(d->canSuspend);
            Q_EMIT canHibernateChanged(d->canHibernate);
            Q_EMIT canHybridSleepChanged(d->canHybridSleep);
        } break;
        case DaemonMessages::HostName: {
            qCDebug(treelandGreeter) << "Message received from daemon: HostName";

            // read host name
            input >> d->hostName;

            // Q_EMIT signal
            Q_EMIT hostNameChanged(d->hostName);
        } break;
        case DaemonMessages::LoginSucceeded: {
            QString user;
            input >> user;

            qCDebug(treelandGreeter) << "Message received from daemon: LoginSucceeded:" << user;

            Q_EMIT loginSucceeded(user);
        } break;
        case DaemonMessages::LoginFailed: {
            QString user;
            input >> user;

            qCDebug(treelandGreeter) << "Message received from daemon: LoginFailed" << user;

            Q_EMIT loginFailed(user);
        } break;
        case DaemonMessages::InformationMessage: {
            QString message;
            input >> message;

            qCDebug(treelandGreeter) << "Information Message received from daemon: " << message;
            Q_EMIT informationMessage(message);
        } break;
        case DaemonMessages::SwitchToGreeter: {
            qCInfo(treelandGreeter) << "switch to greeter";
            Helper::instance()->showLockScreen();
            Q_EMIT switchToGreeter();
        } break;
        case DaemonMessages::UserActivateMessage: {
            QString user;
            int sessionId;
            input >> user >> sessionId;

            // NOTE: maybe DDM will active dde user.
            if (!d->userModel->getUser(user)) {
                qCInfo(treelandGreeter) << "activate user, but switch to greeter";
                Helper::instance()->showLockScreen();
                Q_EMIT switchToGreeter();
                break;
            }

            d->userModel->setCurrentUserName(user);

            qCInfo(treelandGreeter) << "activate successfully: " << user << ", XDG_SESSION_ID: " << sessionId;
        } break;
        case DaemonMessages::UserLoggedIn: {
            QString user;
            int sessionId;
            input >> user >> sessionId;

            // This will happen after a crash recovery of treeland
            qCInfo(treelandGreeter) << "User " << user << " is already logged in";
            auto userPtr = d->userModel->getUser(user);
            if (userPtr) {
                userPtr.get()->setLogined(true);
                Q_EMIT d->userModel->userLoggedIn(user, sessionId);
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

bool GreeterProxy::localValidation(const QString &user, const QString &password) const
{
    auto utf8Password = password.toUtf8();
    struct pam_conv conv = {
        []([[maybe_unused]] int num_msg,
           [[maybe_unused]] const struct pam_message **msg,
           struct pam_response **resp,
           void *appdata_ptr) {
            // pam uses free, we must malloc
            auto *reply = static_cast<pam_response *>(malloc(sizeof(pam_response)));
            reply->resp = strdup(static_cast<const char *>(appdata_ptr)); // 将密码传递给PAM
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

void GreeterProxy::updateAuthSocket()
{
    const QString &socket = d->ddmDisplayManager->AuthInfo();

    if (d->socket->state() == QLocalSocket::ConnectedState) {
        d->socket->disconnectFromServer();
    }

    d->socket->connectToServer(socket);
}

void GreeterProxy::updateLocketState()
{
    if (!d->userModel)
        return;
    qCInfo(treelandGreeter) << "Update lock state";
    bool locked = false;
    if (auto user = d->userModel->currentUser()) {
        locked = user->logined();
    }

    if (d->isLocked != locked) {
        d->isLocked = locked;
        Q_EMIT isLockedChanged();
    }
}
