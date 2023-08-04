#pragma once

#include <QGuiApplication>

class QLocalSocket;

class SocketHelper : public QGuiApplication {
    Q_OBJECT
public:
    explicit SocketHelper(int argc, char* argv[]);

Q_SIGNALS:
    void socketDisconnected();

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    QLocalSocket* m_socket;
};
