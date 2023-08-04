#include <QGuiApplication>

#include <QLocalSocket>

class QQmlApplicationEngine;

namespace TreeLand {
class WaylandSocketProxy;
class TreeLand : public QGuiApplication
{
    Q_OBJECT
public:
    explicit TreeLand(int argc, char* argv[]);

Q_SIGNALS:
    void socketDisconnected();
    void requestAddNewSocket(const QString &username, int fd);

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    void setup();

private:
    bool m_isTestMode;
    QLocalSocket *m_socket;
    QLocalSocket *m_helperSocket;
    QQmlApplicationEngine *m_engine;
    WaylandSocketProxy *m_socketProxy;
};
}
