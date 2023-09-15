// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "WaylandDisplayServer.h"

#include <QMap>
#include <QDBusVariant>

class QLocalSocket;
class QLocalServer;

namespace SDDM {
    class SocketServer;
}

class SingleWaylandDisplayServer : public SDDM::WaylandDisplayServer
{
    Q_OBJECT
    Q_DISABLE_COPY(SingleWaylandDisplayServer)
public:
    explicit SingleWaylandDisplayServer(SDDM::SocketServer *socketServer, SDDM::Display *parent);

Q_SIGNALS:
    void createWaylandSocketFinished();

public Q_SLOTS:
    void createWaylandSocket(const QString &user);
    void deleteWaylandSocket(const QString &user);
    void activateUser(const QString &user);
    QString getUserWaylandSocket(const QString &user) const;

private:
    SDDM::SocketServer *m_socketServer;
    QLocalServer *m_helperServer;
    QLocalSocket *m_helperSocket;
    QList<QLocalSocket*> m_greeterSockets;
    QMap<QString, QString> m_waylandSockets;
    QProcess *m_helper;
};

