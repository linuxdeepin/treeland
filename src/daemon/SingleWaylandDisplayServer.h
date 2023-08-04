#include "WaylandDisplayServer.h"

#include <QMap>

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
    QString getUserWaylandSocket(const QString &user) const;

private:
    SDDM::SocketServer *m_socketServer;
    QLocalServer *m_helperServer;
    QLocalSocket *m_greeterSocket;
    QLocalSocket *m_helperSocket;
    QMap<QString, QString> m_waylandSockets;
    QProcess *m_helper;
};

