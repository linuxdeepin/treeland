#include "treeland.h"
#include "MessageHandler.h"
#include "Messages.h"
#include "SocketWriter.h"
#include "helper.h"
#include "SafeDataStream.h"
#include "qwdisplay.h"
#include "wquickbackend_p.h"
#include "wquicksocket_p.h"

#include "waylandserver.h"

#include <WServer>
#include <WSurface>
#include <WSeat>
#include <WCursor>
#include <wsocket.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>
#include <QLoggingCategory>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

Q_LOGGING_CATEGORY(debug, "treeland.kernel.debug", QtDebugMsg);

using namespace SDDM;

namespace TreeLand {

class Login : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    Q_INVOKABLE void login() {
        emit requestLogin();
    }

signals:
    void requestLogin();
};

TreeLand::TreeLand(int argc, char* argv[])
    : QGuiApplication(argc, argv)
{
    connect(this, &TreeLand::requestAddNewSocket, this, &TreeLand::addNewSocket);

    QCommandLineOption socket({"s", "socket"}, "set ddm socket", "socket");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption(socket);

    parser.process(*this);

    qCDebug(debug) << "socket: " << parser.value(socket);

    m_socket = new QLocalSocket(this);

    connect(m_socket, &QLocalSocket::connected, this, &TreeLand::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &TreeLand::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &TreeLand::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &TreeLand::error);

    setup();

    m_socket->connectToServer(parser.value(socket));
}

void TreeLand::setup()
{
    m_engine = new QQmlApplicationEngine(this);
    m_engine->load(QUrl(u"qrc:/TreeLand/Main.qml"_qs));

    Login* login = new Login;
    connect(login, &Login::requestLogin, this, [=] {
        Session::Type type = Session::Type::WaylandSession;
        QString name("gedit");
        Session session(type, name);
        SocketWriter(m_socket) << quint32(GreeterMessages::Login) << "lxz" << "960322zdY." << session;
    });
    m_engine->rootContext()->setContextProperty("login", login);

    WServer *server = m_engine->rootObjects().first()->findChild<WServer*>();
    treeland_socket_manager_v1_create(server->handle()->handle(), this);
}

void TreeLand::addNewSocket(int fd) {
    qInfo() << Q_FUNC_INFO << fd;
    WServer *server = m_engine->rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    WSocket* socket = new WSocket(true);
    socket->create(fd, false);
    server->addSocket(socket);

    auto backend = server->findChild<WQuickBackend*>();
    Q_ASSERT(backend);
}

void TreeLand::connected() {
    // log connection
    qDebug() << "Connected to the daemon.";

    WQuickSocket *socket = m_engine->rootObjects().first()->findChild<WQuickSocket*>();
    Q_ASSERT(socket);

    // send connected message
    SocketWriter(m_socket) << quint32(GreeterMessages::Connect) << socket->socketFile();
}

void TreeLand::disconnected() {
    // log disconnection
    qDebug() << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();

    qDebug() << "Display Manager is closed socket connect, quiting treeland.";
    exit();
}

void TreeLand::error() {
    qCritical() << "Socket error: " << m_socket->errorString();
}

void TreeLand::readyRead() {
    // input stream
    QDataStream input(m_socket);

    while (input.device()->bytesAvailable()) {
        // read message
        quint32 message;
        input >> message;

        switch (DaemonMessages(message)) {
            case DaemonMessages::Capabilities: {
                // log message
                qDebug() << "Message received from daemon: Capabilities";
            }
            break;
            default:
            break;
        }
    }
}
}

int main (int argc, char *argv[]) {
    qInstallMessageHandler(GreeterMessageHandler);

    WServer::initializeQPA();
    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    TreeLand::TreeLand treeland(argc, argv);

    return treeland.exec();
}

#include "treeland.moc"
