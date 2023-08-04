#include "SingleWaylandDisplayServer.h"
#include "Messages.h"
#include "SocketServer.h"
#include "Constants.h"
#include "SocketWriter.h"
#include "Utils.h"

#include <QStandardPaths>
#include <QChar>
#include <QLocalSocket>
#include <QLocalServer>
#include <QDataStream>
#include <QTimer>

#include <fcntl.h>
#include <sys/socket.h>

using namespace SDDM;

SingleWaylandDisplayServer::SingleWaylandDisplayServer(SocketServer *socketServer, Display *parent)
    : SDDM::WaylandDisplayServer(parent)
    , m_socketServer(socketServer)
    , m_helperServer(new QLocalServer(this))
    , m_greeterSocket(nullptr)
    , m_helperSocket(nullptr)
    , m_helper(new QProcess(this))
{
    m_helper->setProgram(QString("%1/treeland-helper").arg(QStringLiteral(LIBEXEC_INSTALL_DIR)));

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
                    case TreelandMessages::NewWaylandSocket: {
                        QString username;
                        QString path;
                        input >> username >> path;
                        qInfo() << username << path;

                        m_waylandSockets[username] = path;
                        emit createWaylandSocketFinished();
                    }
                    break;
                    default:
                    break;
                }
            }
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    });

    connect(m_socketServer, &SocketServer::connected, this, [=](QLocalSocket *socket, const QString &path) {
        m_greeterSocket = socket;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", path);
        m_helper->setArguments({"--socket", m_helperServer->fullServerName()});
        m_helper->setProcessEnvironment(env);
        m_helper->startDetached();
    });
}

void SingleWaylandDisplayServer::createWaylandSocket(const QString &user) {
    SocketWriter(m_helperSocket) << quint32(DaemonMessages::CreateWaylandSocket) << user;
}

QString SingleWaylandDisplayServer::getUserWaylandSocket(const QString &user) const {
    return m_waylandSockets.value(user);
}

// TODO: add remove socket
