// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <wquickwaylandserver.h>

#include "protocols/socket_manager_impl.h"

class TreeLandHelper;
namespace Waylib::Server {
    class WServer;
}

class SocketManager : public Waylib::Server::WQuickWaylandServerInterface {
    Q_OBJECT

    QML_ELEMENT

public:
    explicit SocketManager(QObject *parent = nullptr);

    treeland_socket_manager_v1 *impl();

Q_SIGNALS:
    void newSocket(const QString &username, int fd);

protected:
    void create() override;

private:
    treeland_socket_manager_v1 *m_impl;
};