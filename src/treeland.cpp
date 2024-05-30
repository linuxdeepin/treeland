// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland.h"

#include "DisplayManager.h"
#include "DisplayManagerSession.h"
#include "MessageHandler.h"
#include "Messages.h"
#include "SignalHandler.h"
#include "SocketWriter.h"
#include "compositor1adaptor.h"
#include "helper.h"

#include <WCursor>
#include <WSeat>
#include <WServer>
#include <WSurface>
#include <wordexp.h>
#include <wrenderhelper.h>
#include <wsocket.h>

#include <qwbackend.h>
#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwlogging.h>

#include <DLog>

#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QTimer>
#include <qdbusconnection.h>
#include <qdbusunixfiledescriptor.h>
#include <qqmlextensionplugin.h>

#include <pwd.h>

Q_LOGGING_CATEGORY(debug, "treeland.kernel.debug", QtDebugMsg);

using namespace DDM;
DCORE_USE_NAMESPACE;

QString unescape(const QString &str) noexcept
{
    QString unescapedStr;
    for (qsizetype i = 0; i < str.size(); ++i) {
        auto c = str.at(i);
        if (c != '\\') {
            unescapedStr.append(c);
            continue;
        }

        switch (str.at(i + 1).toLatin1()) {
        default:
            unescapedStr.append(c);
            break;
        case 'n':
            unescapedStr.append('\n');
            ++i;
            break;
        case 't':
            unescapedStr.append('\t');
            ++i;
            break;
        case 'r':
            unescapedStr.append('\r');
            ++i;
            break;
        case '\\':
            unescapedStr.append('\\');
            ++i;
            break;
        case ';':
            unescapedStr.append(';');
            ++i;
            break;
        case 's': {
            unescapedStr.append(R"(\ )");
            ++i;
        } break;
        }
    }

    return unescapedStr;
}

std::optional<QStringList> unescapeExecArgs(const QString &str) noexcept
{
    auto unescapedStr = unescape(str);
    if (unescapedStr.isEmpty()) {
        qWarning() << "unescape Exec failed.";
        return std::nullopt;
    }

    auto deleter = [](wordexp_t *word) {
        wordfree(word);
        delete word;
    };
    std::unique_ptr<wordexp_t, decltype(deleter)> words{ new (std::nothrow)
                                                             wordexp_t{ 0, nullptr, 0 },
                                                         deleter };

    if (auto ret = wordexp(unescapedStr.toLocal8Bit(), words.get(), WRDE_SHOWERR); ret != 0) {
        QString errMessage;
        switch (ret) {
        case WRDE_BADCHAR:
            errMessage = "BADCHAR";
            break;
        case WRDE_BADVAL:
            errMessage = "BADVAL";
            break;
        case WRDE_CMDSUB:
            errMessage = "CMDSUB";
            break;
        case WRDE_NOSPACE:
            errMessage = "NOSPACE";
            break;
        case WRDE_SYNTAX:
            errMessage = "SYNTAX";
            break;
        default:
            errMessage = "unknown";
        }
        qWarning() << "wordexp error: " << errMessage;
        return std::nullopt;
    }

    QStringList execList;
    for (std::size_t i = 0; i < words->we_wordc; ++i) {
        execList.emplace_back(words->we_wordv[i]);
    }

    return execList;
}

namespace TreeLand {
TreeLand::TreeLand(TreeLandAppContext context)
    : QObject()
    , m_context(context)
{
    if (!context.socket.isEmpty()) {
        qInstallMessageHandler(DDM::GreeterMessageHandler);

        new DDM::SignalHandler;
    }

    m_socket = new QLocalSocket(this);

    connect(m_socket, &QLocalSocket::connected, this, &TreeLand::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &TreeLand::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &TreeLand::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &TreeLand::error);

    setup();
    Helper *helper = m_engine->singletonInstance<Helper *>("TreeLand.Utils", "Helper");

    if (!context.socket.isEmpty()) {
        Q_ASSERT(helper);
        auto connectToServer = [this, context] {
            m_socket->connectToServer(context.socket);
        };

        connect(helper, &Helper::socketFileChanged, this, connectToServer);

        if (!helper->socketFile().isEmpty()) {
            connectToServer();
        }
    }

    if (!context.run.isEmpty()) {
        qInfo() << "run cmd:" << context.run;
        auto exec = [runCmd = context.run, helper] {
            if (auto cmdline = unescapeExecArgs(runCmd); cmdline) {
                auto cmdArgs = cmdline.value();

                auto envs = QProcessEnvironment::systemEnvironment();
                envs.insert("WAYLAND_DISPLAY", helper->socketFile());

                QProcess process;
                process.setProgram(cmdArgs.constFirst());
                process.setArguments(cmdArgs.mid(1));
                process.setProcessEnvironment(envs);
                process.startDetached();
            }
        };
        auto con =
            connect(helper, &Helper::socketFileChanged, this, exec, Qt::SingleShotConnection);

        if (!helper->socketFile().isEmpty()) {
            QObject::disconnect(con);
            exec();
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
    WRenderHelper::setupRendererBackend();
    m_engine = new QQmlApplicationEngine(this);
    m_engine->addUrlInterceptor(new DtkInterceptor(this));
    m_engine->rootContext()->setContextProperty("TreeLand", this);
    m_engine->loadFromModule("TreeLand", "Main");
}

bool TreeLand::testMode() const
{
    return m_context.socket.isEmpty();
}

void TreeLand::connected()
{
    // log connection
    qDebug() << "Connected to the daemon.";

    Helper *helper = m_engine->singletonInstance<Helper *>("TreeLand.Utils", "Helper");
    Q_ASSERT(helper);

    connect(helper, &Helper::backToNormal, this, [this] {
        DDM::SocketWriter(m_socket) << quint32(DDM::GreeterMessages::BackToNormal);
    });
    connect(helper, &Helper::reboot, this, [this] {
        DDM::SocketWriter(m_socket) << quint32(DDM::GreeterMessages::Reboot);
    });

    // send connected message
    DDM::SocketWriter(m_socket) << quint32(DDM::GreeterMessages::Connect);
}

void TreeLand::setPersonalManager(QuickPersonalizationManager *manager)
{
    m_personalManager = manager;
}

void TreeLand::retranslate() noexcept
{
    m_engine->retranslate();
}

void TreeLand::disconnected()
{
    // log disconnection
    qDebug() << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();

    qDebug() << "Display Manager is closed socket connect, quiting treeland.";
    qApp->exit();
}

void TreeLand::error()
{
    qCritical() << "Socket error: " << m_socket->errorString();
}

void TreeLand::readyRead()
{
    // input stream
    QDataStream input(m_socket);

    while (input.device()->bytesAvailable()) {
        // read message
        quint32 message;
        input >> message;

        switch (DDM::DaemonMessages(message)) {
        case DDM::DaemonMessages::Capabilities: {
            // log message
            qDebug() << "Message received from daemon: Capabilities";
        } break;
        case DDM::DaemonMessages::UserActivateMessage: {
            QString user;
            input >> user;

            for (auto key : m_userWaylandSocket.keys()) {
                m_userWaylandSocket[key]->setEnabled(key == user);
            }

            struct passwd *pwd = getpwnam(user.toUtf8());
            m_personalManager->setCurrentUserId(pwd->pw_uid);
        } break;
        case DDM::DaemonMessages::SwitchToGreeter: {
            Helper *helper = m_engine->singletonInstance<Helper *>("TreeLand.Utils", "Helper");

            Q_ASSERT(helper);
            Q_EMIT helper->greeterVisibleChanged();
        } break;
        default:
            break;
        }
    }
}

bool TreeLand::Activate(QDBusUnixFileDescriptor _fd)
{
    if (!_fd.isValid()) {
        return false;
    }

    auto fd = std::make_shared<QDBusUnixFileDescriptor>();
    fd->swap(_fd);

    auto socket = std::make_shared<WSocket>(true);
    socket->create(fd->fileDescriptor(), false);

    WServer *server = m_engine->rootObjects().first()->findChild<WServer *>();
    server->addSocket(socket.get());

    auto uid = connection().interface()->serviceUid(message().service());
    struct passwd *pw;
    pw = getpwuid(uid);
    QString user{ pw->pw_name };
    m_userWaylandSocket[user] = socket;
    m_userDisplayFds[user] = fd;

    DisplayManager manager("org.freedesktop.DisplayManager",
                           "/org/freedesktop/DisplayManager",
                           QDBusConnection::systemBus());

    const auto sessionPath = manager.lastSession();
    if (!sessionPath.path().isEmpty()) {
        DisplaySession session(manager.service(), sessionPath.path(), QDBusConnection::systemBus());
        socket->setEnabled(session.userName() == user);
    }

    connect(connection().interface(),
            &QDBusConnectionInterface::serviceUnregistered,
            socket.get(),
            [this, user] {
                m_userWaylandSocket.remove(user);
                m_userDisplayFds.remove(user);
            });

    return true;
}

} // namespace TreeLand

int main(int argc, char *argv[])
{
    QWLog::init();
    WServer::initializeQPA();
    // QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QGuiApplication app(argc, argv);
    app.setOrganizationName("deepin");
    app.setApplicationName("treeland");

    DLogManager::registerConsoleAppender();
    DLogManager::registerJournalAppender();
    DLogManager::registerFileAppender();

    QCommandLineOption socket({ "s", "socket" }, "set ddm socket", "socket");
    QCommandLineOption run({ "r", "run" }, "run a process", "run");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({ socket, run });

    parser.process(app);

    TreeLand::TreeLand treeland({ parser.value(socket), parser.value(run) });
    new Compositor1Adaptor(&treeland);

    QDBusConnection::systemBus().registerService("org.deepin.Compositor1");
    QDBusConnection::systemBus().registerObject("/org/deepin/Compositor1", &treeland);

    QDBusConnection::sessionBus().registerService("org.deepin.Compositor1");
    QDBusConnection::sessionBus().registerObject("/org/deepin/Compositor1", &treeland);

    return app.exec();
}
