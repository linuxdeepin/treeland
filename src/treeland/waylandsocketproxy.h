// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QMap>

#include <memory>
#include <QQmlEngine>
#include <wquickwaylandserver.h>

namespace Waylib::Server {
    class WSocket;
}

namespace TreeLand {
class WaylandSocketProxy : public Waylib::Server::WQuickWaylandServerInterface {
    Q_OBJECT

    QML_ELEMENT

public:
    explicit WaylandSocketProxy(QObject *parent = nullptr);

    Q_INVOKABLE void newSocket(const QString &username, int fd);
    Q_INVOKABLE void deleteSocket(const QString &username);

    Q_INVOKABLE void activateUser(const QString &username);

    Q_INVOKABLE QString user(std::shared_ptr<Waylib::Server::WSocket> socket) const;

Q_SIGNALS:
    void socketCreated(std::shared_ptr<Waylib::Server::WSocket> socket);
    void socketDeleted(std::shared_ptr<Waylib::Server::WSocket> socket);
    void userActivated(const QString &username);

private:
    QMap<QString, std::shared_ptr<Waylib::Server::WSocket>> m_userWaylandSocket;
};
}
