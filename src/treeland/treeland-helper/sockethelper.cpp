// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sockethelper.h"

#include "SocketWriter.h"
#include "Messages.h"

#include <QDebug>
#include <QObject>
#include <QWindow>

#include <QLocalSocket>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <wsocket.h>

#include "qwayland-treeland-socket-manager-v1.h"

using namespace SDDM;

WAYLIB_SERVER_USE_NAMESPACE

SocketManager::SocketManager()
    : QWaylandClientExtensionTemplate<SocketManager>(1)
{

}

SocketContext::SocketContext(struct ::treeland_socket_context_v1 *object)
    : QWaylandClientExtensionTemplate<SocketContext>(1)
    , QtWayland::treeland_socket_context_v1(object)
{

}

SocketHelper::SocketHelper(int argc, char* argv[])
    : QGuiApplication(argc, argv)
    , m_socketManager(new SocketManager())
{
    const QStringList args = QCoreApplication::arguments();
    QString server;
    int pos;

    if ((pos = args.indexOf(QStringLiteral("--socket"))) >= 0) {
        if (pos >= args.length() - 1) {
            qCritical() << "This application is not supposed to be executed manually";
            return;
        }
        server = args[pos + 1];
    }

    m_socket = new QLocalSocket;

    connect(m_socket, &QLocalSocket::connected, this, &SocketHelper::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &SocketHelper::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &SocketHelper::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &SocketHelper::error);

    connect(m_socketManager, &SocketManager::activeChanged, this, [=] {
        if (m_socketManager->isActive()) {
            m_socket->connectToServer(server);
        }
    });

    emit m_socketManager->activeChanged();
}

void SocketHelper::connected() {
    qDebug() << "[treeland helper] Connected to the daemon.";
    SocketWriter(m_socket) << quint32(TreelandMessages::Connect);
}

void SocketHelper::disconnected() {
    qDebug() << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();

    qDebug() << "Display Manager is closed socket connect, quiting treeland.";
    exit();
}

void SocketHelper::readyRead() {
    // input stream
    QDataStream input(m_socket);

    while (input.device()->bytesAvailable()) {
        // read message
        quint32 message;
        input >> message;

        switch (DaemonMessages(message)) {
            case DaemonMessages::CreateWaylandSocket: {
                QString username;
                input >> username;

                auto socket = std::make_shared<WSocket>(true);
                struct passwd *pw = getpwnam(username.toUtf8());
                const QString &path = QString("/run/user/%1").arg(pw->pw_uid);

                QDir().mkpath(path);
                chown(path.toUtf8(), pw->pw_uid, pw->pw_gid);

                socket->autoCreate(path);
                chown(socket->fullServerName().toUtf8(), pw->pw_uid, pw->pw_gid);

                auto context = std::make_shared<SocketContext>(m_socketManager->create());
                context->set_username(username);
                context->set_fd(socket->socketFd());
                context->commit();
                context->setProperty("username", username);

                SocketWriter(m_socket) << quint32(TreelandMessages::CreateWaylandSocket) << username << socket->fullServerName();

                m_userSocket.insert(std::move(context), std::move(socket));
            }
            break;
            case DaemonMessages::DeleteWaylandSocket: {
                QString user;
                input >> user;

                for (auto it = m_userSocket.begin(); it != m_userSocket.end(); ++it) {
                    const QString username = it.key().get()->property("username").toString();
                    if (username == user) {
                        SocketWriter(m_socket) << quint32(TreelandMessages::DeleteWaylandSocket) << username;
                        m_userSocket.erase(it);
                        break;
                    }
                }
            }
            break;
            default:
            break;
        }
    }
}

void SocketHelper::error() {
    qCritical() << "Socket error: " << m_socket->errorString();
}

// TODO: 接受 socket，从环境变量中连接上 WAYLAND，走 wayland 协议为用户创建 socket。
int main (int argc, char *argv[]) {
    Q_ASSERT(::getuid() == 0);
    if (argc != 3) {
        QTextStream(stderr) << "Wrong number of arguments\n";
        return -1;
    }

    SocketHelper helper(argc, argv);

    return helper.exec();
}
