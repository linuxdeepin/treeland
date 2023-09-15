// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "SingleWaylandDisplayServer.h"
#include "Messages.h"
#include "SocketServer.h"
#include "Constants.h"
#include "SocketWriter.h"
#include "Utils.h"
#include "Display.h"

#include <QStandardPaths>
#include <QChar>
#include <QLocalSocket>
#include <QLocalServer>
#include <QDataStream>
#include <QTimer>
#include <QProcessEnvironment>

#include <fcntl.h>
#include <sys/socket.h>

using namespace SDDM;

SingleWaylandDisplayServer::SingleWaylandDisplayServer(SocketServer *socketServer, Display *parent)
    : SDDM::WaylandDisplayServer(parent)
    , m_socketServer(socketServer)
    , m_helperServer(new QLocalServer(this))
    , m_helperSocket(nullptr)
    , m_helper(new QProcess(this))
{
    QProcess *m_seatd = new QProcess(this);
    m_seatd->setProgram("seatd");
    m_seatd->setArguments({"-u", "dde", "-g", "dde", "-l", "debug"});
    m_seatd->setProcessEnvironment([] {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("SEATD_VTBOUND", "0");
        return env;
    }());
    connect(m_seatd, &QProcess::readyReadStandardOutput, this, [m_seatd] {
        qInfo() << m_seatd->readAllStandardOutput();
    });
    connect(m_seatd, &QProcess::readyReadStandardError, this, [m_seatd] {
        qWarning() << m_seatd->readAllStandardError();
    });

    m_seatd->start();

    QString socketName = QStringLiteral("treeland-helper-%1").arg(generateName(6));

    // set server options
    m_helperServer->setSocketOptions(QLocalServer::UserAccessOption);

    // start listening
    if (!m_helperServer->listen(socketName)) {
        // log message
        qCritical() << "Failed to start socket server.";
        // return fail
        return;
    }

    // log message
    qDebug() << "Socket server started.";

    // connect signals
    connect(m_helperServer, &QLocalServer::newConnection, this, [=] {
        QLocalSocket *socket = m_helperServer->nextPendingConnection();

        // connect signals
        connect(socket, &QLocalSocket::readyRead, this, [=] {
            QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());
            QDataStream input(socket);

            while (input.device()->bytesAvailable()) {
                // read message
                quint32 message;
                input >> message;

                switch (TreelandMessages(message)) {
                    case TreelandMessages::Connect: {
                        m_helperSocket = socket;
                    }
                    break;
                    case TreelandMessages::CreateWaylandSocket: {
                        QString username;
                        QString path;
                        input >> username >> path;

                        m_waylandSockets[username] = path;
                        emit createWaylandSocketFinished();
                        for (auto greeter : m_greeterSockets) {
                            SocketWriter(greeter) << quint32(DaemonMessages::WaylandSocketCreated) << username;
                        }
                    }
                    break;
                    case TreelandMessages::DeleteWaylandSocket: {
                        QString username;
                        input >> username;

                        qDebug() << Q_FUNC_INFO << "wayland socket deleted.";

                        m_waylandSockets.remove(username);
                        for (auto greeter : m_greeterSockets) {
                            SocketWriter(greeter) << quint32(DaemonMessages::WaylandSocketDeleted) << username;
                        }
                    }
                    break;
                    default:
                    break;
                }
            }
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    });

    connect(m_socketServer, &SocketServer::connected, this, [=](QLocalSocket *socket) {
        m_greeterSockets << socket;
    });
    connect(m_socketServer, &SocketServer::requestStartHelper, this, [this](QLocalSocket *, const QString &path) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", path);
        m_helper->setProgram(QString("%1/treeland-helper").arg(QStringLiteral(LIBEXEC_INSTALL_DIR)));
        m_helper->setArguments({"--socket", m_helperServer->fullServerName()});
        m_helper->setProcessEnvironment(env);
        m_helper->start();
    });
    connect(m_socketServer, &SocketServer::disconnected, this, [=](QLocalSocket *socket) {
        m_greeterSockets.removeOne(socket);
    });

    // TODO: use PAM auth again
    connect(m_socketServer, &SocketServer::requestActivateUser, this, [this](QLocalSocket *socket, const QString &user){
        activateUser(user);
    });
}

void SingleWaylandDisplayServer::activateUser(const QString &user) {
    for (auto greeter : m_greeterSockets) {
        if (user == "dde") {
            SocketWriter(greeter) << quint32(DaemonMessages::SwitchToGreeter);
        }
        else {
            SocketWriter(greeter) << quint32(DaemonMessages::UserActivateMessage) << user;
            displayPtr()->activateUser(user); // IOCTL activate
        }
    }
}

void SingleWaylandDisplayServer::createWaylandSocket(const QString &user) {
    SocketWriter(m_helperSocket) << quint32(DaemonMessages::CreateWaylandSocket) << user;
}

void SingleWaylandDisplayServer::deleteWaylandSocket(const QString &user) {
    SocketWriter(m_helperSocket) << quint32(DaemonMessages::DeleteWaylandSocket) << user;
}

QString SingleWaylandDisplayServer::getUserWaylandSocket(const QString &user) const {
    return m_waylandSockets.value(user);
}
