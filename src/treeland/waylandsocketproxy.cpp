// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "waylandsocketproxy.h"

#include <wsocket.h>

#include <QDebug>

WAYLIB_SERVER_USE_NAMESPACE

namespace TreeLand {
WaylandSocketProxy::WaylandSocketProxy(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
{}

void WaylandSocketProxy::newSocket(const QString &username, int fd)
{
    auto socket = std::make_shared<WSocket>(true);
    m_userWaylandSocket[username] = socket;
    socket->create(fd, false);

    emit socketCreated(socket);

    server()->addSocket(socket.get());
}

QString WaylandSocketProxy::user(std::shared_ptr<Waylib::Server::WSocket> socket) const {
    return m_userWaylandSocket.key(socket);
}

void WaylandSocketProxy::deleteSocket(const QString &username)
{
    if (m_userWaylandSocket.count(username)) {
        auto socket = m_userWaylandSocket.value(username);
        m_userWaylandSocket.remove(username);
        emit socketDeleted(socket);
    }
}

void WaylandSocketProxy::activateUser(const QString &username)
{
    for (auto it = m_userWaylandSocket.begin(); it != m_userWaylandSocket.end(); ++it) {
        it.value()->setEnabled(it.key() == username);
    }

    emit userActivated(username);
}
}  // namespace TreeLand
