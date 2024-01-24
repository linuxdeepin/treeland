// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland.h"
#include "MessageHandler.h"
#include "Messages.h"
#include "SocketWriter.h"
#include "helper.h"
#include "waylandsocketproxy.h"
#include "SignalHandler.h"

#include <WServer>
#include <WSurface>
#include <WSeat>
#include <WCursor>
#include <qqmlextensionplugin.h>
#include <wsocket.h>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwcompositor.h>

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

#include <pwd.h>

Q_IMPORT_PLUGIN(TreeLand_ProtocolsPlugin)
Q_IMPORT_PLUGIN(TreeLand_UtilsPlugin)

Q_LOGGING_CATEGORY(debug, "treeland.kernel.debug", QtDebugMsg);

using namespace SDDM;

namespace TreeLand {
TreeLand::TreeLand(TreeLandAppContext context)
    : QObject()
    , m_context(context)
{
    if (!context.socket.isEmpty()) {
        qInstallMessageHandler(GreeterMessageHandler);

        new SignalHandler;
    }

    m_socket = new QLocalSocket(this);

    connect(m_socket, &QLocalSocket::connected, this, &TreeLand::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &TreeLand::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &TreeLand::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &TreeLand::error);

    setup();

    if (!context.socket.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        Helper *helper = m_engine->singletonInstance<Helper*>("TreeLand.Utils", "Helper");
#else
        auto helperTypeId = qmlTypeId("TreeLand.Utils", 1, 0, "Helper");
        Helper *helper = m_engine->singletonInstance<Helper*>(helperTypeId);
#endif
        Q_ASSERT(helper);
        auto connectToServer = [this, context] {
            m_socket->connectToServer(context.socket);
        };

        connect(helper, &Helper::socketFileChanged, this, connectToServer);

        if (!helper->socketFile().isEmpty()) {
            connectToServer();
        }
    }
}

class DtkInterceptor : public QObject, public QQmlAbstractUrlInterceptor
{
public:
    DtkInterceptor(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    QUrl intercept(const QUrl &path, DataType type)
    {
        if (type != DataType::QmlFile)
            return path;
        if (path.path().endsWith("overridable/InWindowBlur.qml")) {
            qDebug() << "Override dtk's InWindowBlur";
            return QStringLiteral("qrc:/treeland/override/dtk/InWindowBlur.qml");
        }

        return path;
    }
};

void TreeLand::setup()
{
    m_engine = new QQmlApplicationEngine(this);
    m_engine->addUrlInterceptor(new DtkInterceptor(this));
    m_engine->rootContext()->setContextProperty("TreeLand", this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    m_engine->loadFromModule("TreeLand", "Main");
#else
    m_engine->addImportPath(":/qt/qml");
    m_engine->load(QUrl(u"qrc:/qt/qml/TreeLand/Main.qml"_qs));
#endif
}

bool TreeLand::testMode() const {
    return m_context.socket.isEmpty();
}

void TreeLand::connected() {
    // log connection
    qDebug() << "Connected to the daemon.";

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    Helper *helper = m_engine->singletonInstance<Helper*>("TreeLand.Utils", "Helper");
#else
    auto helperTypeId = qmlTypeId("TreeLand.Utils", 1, 0, "Helper");
    Helper *helper = m_engine->singletonInstance<Helper*>(helperTypeId);
#endif
    Q_ASSERT(helper);

    connect(helper, &Helper::backToNormal, this, [this] {
        SocketWriter(m_socket) << quint32(GreeterMessages::BackToNormal);
    });
    connect(helper, &Helper::reboot, this, [this] {
         SocketWriter(m_socket) << quint32(GreeterMessages::Reboot);
    });

    // send connected message
    SocketWriter(m_socket) << quint32(GreeterMessages::Connect);

    SocketWriter(m_socket) << quint32(GreeterMessages::StartHelper) << helper->socketFile();
}

void TreeLand::setSocketProxy(WaylandSocketProxy *socketProxy)
{
    m_socketProxy = socketProxy;
}

void TreeLand::setPersonalManager(QuickPersonalizationManager *manager)
{
    m_personalManager = manager;
}

void TreeLand::retranslate() noexcept {
    m_engine->retranslate();
}

void TreeLand::disconnected() {
    // log disconnection
    qDebug() << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();

    qDebug() << "Display Manager is closed socket connect, quiting treeland.";
    qApp->exit();
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
            case DaemonMessages::WaylandSocketDeleted: {
                QString user;
                input >> user;

                m_socketProxy->deleteSocket(user);
            }
            break;
            case DaemonMessages::UserActivateMessage: {
                QString user;
                input >> user;

                m_socketProxy->activateUser(user);

                struct passwd *pwd = getpwnam(user.toUtf8());
                m_personalManager->setCurrentUserId(pwd->pw_uid);
            }
            break;
            case DaemonMessages::SwitchToGreeter: {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                Helper *helper = m_engine->singletonInstance<Helper*>("TreeLand.Utils", "Helper");
#else
                auto helperTypeId = qmlTypeId("TreeLand", 1, 0, "Helper");
                Helper *helper = m_engine->singletonInstance<Helper*>(helperTypeId);
#endif
                Q_ASSERT(helper);
                Q_EMIT helper->greeterVisibleChanged();
            }
            break;
            default:
            break;
        }
    }
}
}

int main (int argc, char *argv[]) {
    qInstallMessageHandler(SDDM::GreeterMessageHandler);

    WServer::initializeQPA();
    // QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QGuiApplication app(argc, argv);

    QCommandLineOption socket({"s", "socket"}, "set ddm socket", "socket");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption(socket);

    parser.process(app);

    TreeLand::TreeLand treeland({
        parser.value(socket),
    });

    return app.exec();
}
