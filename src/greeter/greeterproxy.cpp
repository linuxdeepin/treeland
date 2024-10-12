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

#include "global.h"
#include "sessionmodel.h"
#include "usermodel.h"

#include <DisplayManager.h>
#include <DisplayManagerSession.h>
#include <Messages.h>
#include <SocketWriter.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLocalSocket>
#include <QVariantMap>

using namespace DDM;

class GreeterProxyPrivate
{
public:
    SessionModel *sessionModel{ nullptr };
    UserModel *userModel{ nullptr };
    QLocalSocket *socket{ nullptr };
    DisplayManager *displayManager{ nullptr };
    QString hostName;
    bool canPowerOff{ false };
    bool canReboot{ false };
    bool canSuspend{ false };
    bool canHibernate{ false };
    bool canHybridSleep{ false };
};

GreeterProxy::GreeterProxy(QObject *parent)
    : QObject(parent)
    , d(new GreeterProxyPrivate())
{
    const QStringList args = QCoreApplication::arguments();
    QString server;
    int pos;

    if ((pos = args.indexOf(QStringLiteral("--socket"))) >= 0) {
        if (pos >= args.length() - 1) {
            return;
        }
        server = args[pos + 1];
    }

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

    // connect to server
    d->socket->connectToServer(server);
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

    auto sessions = d->displayManager->sessions();
    for (auto session : sessions) {
        onSessionAdded(session);
    }
}

void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex) const
{
    if (!d->sessionModel) {
        qCCritical(greeter) << "Session model is not set.";

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
    auto userInfo = userModel()->get(user);
    if (userInfo.isValid()) {
        SocketWriter(d->socket) << quint32(GreeterMessages::Unlock) << user << password;
    }
}

void GreeterProxy::activateUser(const QString &user)
{
    auto userInfo = userModel()->get(user);
    SocketWriter(d->socket) << quint32(GreeterMessages::ActivateUser) << user;
}

void GreeterProxy::connected()
{
    qCDebug(greeter) << "Connected to the daemon.";

    SocketWriter(d->socket) << quint32(GreeterMessages::Connect);
}

void GreeterProxy::disconnected()
{
    qCDebug(greeter) << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();
}

void GreeterProxy::error()
{
    qCCritical(greeter) << "Socket error: " << d->socket->errorString();
}

void GreeterProxy::onSessionAdded(const QDBusObjectPath &session)
{
    DisplaySession s(d->displayManager->service(), session.path(), QDBusConnection::systemBus());

    userModel()->updateUserLoginState(s.userName(), true);
}

void GreeterProxy::onSessionRemoved(const QDBusObjectPath &session)
{
    // FIXME: Reset all user state, because we can't know which user was logout.
    userModel()->clearUserLoginState();

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
            qCDebug(greeter) << "Message received from daemon: Capabilities";

            // read capabilities
            quint32 capabilities;
            input >> capabilities;

            // parse capabilities
            d->canPowerOff = capabilities & Capability::PowerOff;
            d->canReboot = capabilities & Capability::Reboot;
            d->canSuspend = capabilities & Capability::Suspend;
            d->canHibernate = capabilities & Capability::Hibernate;
            d->canHybridSleep = capabilities & Capability::HybridSleep;

            // emit signals
            Q_EMIT canPowerOffChanged(d->canPowerOff);
            Q_EMIT canRebootChanged(d->canReboot);
            Q_EMIT canSuspendChanged(d->canSuspend);
            Q_EMIT canHibernateChanged(d->canHibernate);
            Q_EMIT canHybridSleepChanged(d->canHybridSleep);
        } break;
        case DaemonMessages::HostName: {
            qCDebug(greeter) << "Message received from daemon: HostName";

            // read host name
            input >> d->hostName;

            // emit signal
            Q_EMIT hostNameChanged(d->hostName);
        } break;
        case DaemonMessages::LoginSucceeded: {
            QString user;
            input >> user;

            qCDebug(greeter) << "Message received from daemon: LoginSucceeded:" << user;

            Q_EMIT loginSucceeded(user);
        } break;
        case DaemonMessages::LoginFailed: {
            QString user;
            input >> user;

            qCDebug(greeter) << "Message received from daemon: LoginFailed" << user;

            Q_EMIT loginFailed(user);
        } break;
        case DaemonMessages::InformationMessage: {
            QString message;
            input >> message;

            qCDebug(greeter) << "Information Message received from daemon: " << message;
            Q_EMIT informationMessage(message);
        } break;
        case DaemonMessages::SwitchToGreeter: {
            qCInfo(greeter) << "switch to greeter";
            Q_EMIT switchToGreeter();
        } break;
        case DaemonMessages::UserActivateMessage: {
            QString user;
            input >> user;

            qCInfo(greeter) << "activate successfully: " << user;
        } break;
        default: {
            qCWarning(greeter) << "Unknown message received from daemon." << message;
        }
        }
    }
}
