#pragma once

#include <QGuiApplication>
#include <QMap>
#include <memory>

class QLocalSocket;

namespace Waylib::Server {
class WSocket;
}

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
    QLocalSocket* m_socket;
    QMap<QString, std::shared_ptr<Waylib::Server::WSocket>> m_userSocket;
};
