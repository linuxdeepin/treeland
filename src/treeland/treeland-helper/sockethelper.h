// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QMap>
#include <memory>
#include <QGuiApplication>
#include <QtWaylandClient/QWaylandClientExtension>

#include "qwayland-treeland-socket-manager-v1.h"

class QLocalSocket;

namespace Waylib::Server {
class WSocket;
}

class SocketContext;
class SocketManager : public QWaylandClientExtensionTemplate<SocketManager>, public QtWayland::treeland_socket_manager_v1
{
    Q_OBJECT
public:
    explicit SocketManager();
};

class SocketContext : public QWaylandClientExtensionTemplate<SocketContext>, public QtWayland::treeland_socket_context_v1
{
    Q_OBJECT
public:
    explicit SocketContext(struct ::treeland_socket_context_v1 *object);
};

class SocketHelper : public QGuiApplication {
    Q_OBJECT
public:
    explicit SocketHelper(int argc, char* argv[]);
    ~SocketHelper() = default;

Q_SIGNALS:
    void socketDisconnected();

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    SocketManager* m_socketManager = nullptr;
    QLocalSocket* m_socket;
    QMap<std::shared_ptr<SocketContext>, std::shared_ptr<Waylib::Server::WSocket>> m_userSocket;
};
