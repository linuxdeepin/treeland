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
#include <Messages.h>
#include <SocketWriter.h>
#include <security/pam_appl.h>

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

struct SessionInfo
{
    QString sessionId;
    quint32 uid;
    QString userName;
    QString seat;
    QDBusObjectPath path;
};

static QDBusArgument &operator<<(QDBusArgument &argument, const SessionInfo &info)
{
    argument.beginStructure();
    argument << info.sessionId << info.uid << info.userName << info.seat << info.path;
    argument.endStructure();
    return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument, SessionInfo &info)
{
    argument.beginStructure();
    argument >> info.sessionId >> info.uid >> info.userName >> info.seat >> info.path;
    argument.endStructure();
    return argument;
}

using namespace DDM;

class GreeterProxyPrivate
{
public:
    SessionModel *sessionModel{ nullptr };
    UserModel *userModel{ nullptr };
    QLocalSocket *socket{ nullptr };
    DisplayManager *displayManager{ nullptr };
    org::deepin::DisplayManager *ddmDisplayManager{ nullptr };
    QDBusUnixFileDescriptor authFd;
    QString hostName;
    bool canPowerOff{ false };
    bool canReboot{ false };
    bool canSuspend{ false };
    bool canHibernate{ false };
    bool canHybridSleep{ false };
    bool isLocked{ false };
};

GreeterProxy::GreeterProxy(QObject *parent)
    : QObject(parent)
    , d(new GreeterProxyPrivate())
{
    qDBusRegisterMetaType<SessionInfo>();
    qDBusRegisterMetaType<QList<SessionInfo>>();

    d->displayManager = new DisplayManager("org.freedesktop.DisplayManager",
                                           "/org/freedesktop/DisplayManager",
                                           QDBusConnection::systemBus(),
                                           this);
    d->socket = new QLocalSocket(this);

    // connect signals
    connect(d->socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
    connect(d->socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
    connect(d->socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
    connect(d->socket, &QLocalSocket::errorOccurred, this, &GreeterProxy::error);

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
    connect(d->displayManager, &DisplayManager::SessionAdded, this, &GreeterProxy::onSessionAdded);
    connect(d->displayManager,
            &DisplayManager::SessionRemoved,
            this,
            &GreeterProxy::onSessionRemoved);

    // Use async call to avoid blocking
    QThreadPool::globalInstance()->start([this]() {
        QDBusInterface dbus("org.freedesktop.DBus",
                            "/org/freedesktop/DBus",
                            "org.freedesktop.DBus.Properties",
                            QDBusConnection::systemBus());
        QDBusReply<QList<QDBusObjectPath>> reply = dbus.call("Get", DisplayManager::staticInterfaceName(), "Sessions");
        if (reply.isValid()) {
            auto sessions = reply.value();
            for (auto session : sessions) {
                QMetaObject::invokeMethod(this, &GreeterProxy::onSessionAdded, Qt::QueuedConnection, session);
            }
        }
    });
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
    Session::Type type =
        static_cast<Session::Type>(d->sessionModel->data(index, SessionModel::TypeRole).toInt());
    QString name = d->sessionModel->data(index, SessionModel::FileRole).toString();
    Session session(type, name);
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

static QString getSessionPathByUser(UserPtr userInfo);

void GreeterProxy::logout()
{
    qCDebug(treelandGreeter) << "Logout.";
    auto user = userModel()->currentUser();
    QThreadPool::globalInstance()->start([user]() {
        const auto path = getSessionPathByUser(user);
        if (path.isEmpty()) {
            qCWarning(treelandGreeter, "No session logged in.");
            return;
        }
        qCDebug(treelandGreeter) << "Terminate the session" << path;
        auto reply = DDBusSender::system()
            .service("org.freedesktop.login1")
            .path(path)
            .interface("org.freedesktop.login1.Session")
            .method("Terminate")
            .call();
        if (reply.isError()) {
            qCWarning(treelandGreeter) << "Failed to logout, error:" << reply.error().message();
        }
    });
}

QString getSessionPathByUser(UserPtr userInfo)
{
    if (!userInfo) {
        qCWarning(treelandGreeter) << "No user logged in.";
        return {};
    }

    QDBusPendingReply<QList<SessionInfo>> sessionsRelpy =
        DDBusSender::system()
            .service("org.freedesktop.login1")
            .path("/org/freedesktop/login1")
            .interface("org.freedesktop.login1.Manager")
            .method("ListSessions")
            .call();
    if (sessionsRelpy.isError()) {
        qCWarning(treelandGreeter) << "Failed to logout, error:" << sessionsRelpy.error().message();
        return {};
    }

    QStringList userSessions;
    const auto sessions = sessionsRelpy.value();
    for (auto item : sessions) {
        if (item.uid != userInfo->UID())
            continue;
        // TODO multiple seats.
        if (item.seat.isEmpty())
            continue;
        userSessions << item.path.path();
    }
    std::sort(userSessions.begin(), userSessions.end(), [](const QString &s1, const QString &s2) {
        return s1.localeAwareCompare(s2) > 0;
    });
    for (auto item : userSessions) {
        QDBusPendingReply<QVariant> relpy = DDBusSender::system()
                                                .service("org.freedesktop.login1")
                                                .path(item)
                                                .interface("org.freedesktop.login1.Session")
                                                .property("Active")
                                                .get();
        if (relpy.value().toBool())
            return item;
    }
    return {};
}

void GreeterProxy::activateUser(const QString &user)
{
    auto userInfo = userModel()->get(user);
    SocketWriter(d->socket) << quint32(GreeterMessages::ActivateUser) << user;
}

void GreeterProxy::connected()
{
    qCDebug(treelandGreeter) << "Connected to the daemon.";

    SocketWriter(d->socket) << quint32(GreeterMessages::Connect)
                            << Helper::instance()->defaultWaylandSocket()->fullServerName();
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

void GreeterProxy::onSessionAdded(const QDBusObjectPath &session)
{
    DisplaySession s(d->displayManager->service(), session.path(), QDBusConnection::systemBus());

    userModel()->updateUserLoginState(s.userName(), true);
    updateLocketState();
}

void GreeterProxy::onSessionRemoved([[maybe_unused]] const QDBusObjectPath &session)
{
    // FIXME: Reset all user state, because we can't know which user was logout.
    userModel()->clearUserLoginState();
    updateLocketState();

    auto sessions = d->displayManager->sessions();
    for (auto session : sessions) {
        onSessionAdded(session);
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
            input >> user;

            // NOTE: maybe DDM will active dde user.
            if (!d->userModel->getUser(user)) {
                qCInfo(treelandGreeter) << "activate user, but switch to greeter";
                Helper::instance()->showLockScreen();
                Q_EMIT switchToGreeter();
                break;
            }

            d->userModel->setCurrentUserName(user);

            qCInfo(treelandGreeter) << "activate successfully: " << user;
        } break;
        case DaemonMessages::UserLoggedIn: {
            QString user;
            input >> user;

            // This will happen after a crash recovery of treeland
            qCInfo(treelandGreeter) << "User " << user << " is already logged in";
            auto userPtr = d->userModel->getUser(user);
            if (userPtr) {
                userPtr.get()->setLogined(true);
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
