// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland.h"

#include "cmdline.h"
#include "compositor1adaptor.h"
#include "helper.h"

#include <Messages.h>
#include <SignalHandler.h>
#include <SocketWriter.h>

#include <wsocket.h>
#include <wxwayland.h>

#include <QDebug>
#include <QLocalSocket>
#include <QLoggingCategory>

#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(dbus, "treeland.dbus", QtDebugMsg);

using namespace DDM;
DCORE_USE_NAMESPACE;

namespace Treeland {
Treeland::Treeland(Helper *helper)
    : QObject()
    , m_helper(helper)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::connected, this, &Treeland::connected);
    connect(m_socket, &QLocalSocket::disconnected, this, &Treeland::disconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &Treeland::readyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &Treeland::error);

    if (CmdLine::ref().socket().has_value()) {
        new DDM::SignalHandler(this);
        Q_ASSERT(m_helper);
        auto connectToServer = [this] {
            m_socket->connectToServer(CmdLine::ref().socket().value());
        };

        connect(m_helper, &Helper::socketFileChanged, this, connectToServer);

        WSocket *defaultSocket = m_helper->defaultWaylandSocket();
        if (defaultSocket && defaultSocket->isValid()) {
            connectToServer();
        }
    } else {
        m_helper->setCurrentUserId(getuid());
    }

    if (CmdLine::ref().run().has_value()) {
        auto exec = [runCmd = CmdLine::ref().run().value(), this] {
            qCInfo(dbus) << "run cmd:" << runCmd;
            if (auto cmdline = CmdLine::ref().unescapeExecArgs(runCmd); cmdline) {
                auto cmdArgs = cmdline.value();

                auto envs = QProcessEnvironment::systemEnvironment();
                envs.insert("WAYLAND_DISPLAY", m_helper->defaultWaylandSocket()->fullServerName());
                envs.insert("DISPLAY", m_helper->defaultXWaylandSocket()->displayName());
                envs.insert("DDE_CURRENT_COMPOSITOR", "Treeland");

                QProcess process;
                process.setProgram(cmdArgs.constFirst());
                process.setArguments(cmdArgs.mid(1));
                process.setProcessEnvironment(envs);
                process.setStandardInputFile(QProcess::nullDevice());
                process.setStandardOutputFile(QProcess::nullDevice());
                process.setStandardErrorFile(QProcess::nullDevice());
                process.startDetached();
            }
        };
        auto con =
            connect(m_helper, &Helper::socketFileChanged, this, exec, Qt::SingleShotConnection);

        WSocket *defaultSocket = m_helper->defaultWaylandSocket();
        if (defaultSocket && defaultSocket->isValid()) {
            QObject::disconnect(con);
            exec();
        }
    }

    new Compositor1Adaptor(this);

    // init dbus after QML engine loaded.
    QDBusConnection::systemBus().registerService("org.deepin.Compositor1");
    QDBusConnection::systemBus().registerObject("/org/deepin/Compositor1", this);

    QDBusConnection::sessionBus().registerService("org.deepin.Compositor1");
    QDBusConnection::sessionBus().registerObject("/org/deepin/Compositor1", this);
}

bool Treeland::testMode() const
{
    return !CmdLine::ref().socket().has_value();
}

bool Treeland::debugMode() const
{
#ifdef QT_DEBUG
    return true;
#endif

    return qEnvironmentVariableIsSet("TREELAND_ENABLE_DEBUG");
}

void Treeland::connected()
{
    // log connection
    qCDebug(dbus) << "Connected to the daemon.";

    // send connected message
    DDM::SocketWriter(m_socket) << quint32(DDM::GreeterMessages::Connect);
}

void Treeland::retranslate() noexcept
{
    // m_engine->retranslate();
}

void Treeland::disconnected()
{
    // log disconnection
    qCDebug(dbus) << "Disconnected from the daemon.";

    Q_EMIT socketDisconnected();

    qCDebug(dbus) << "Display Manager is closed socket connect, quiting treeland.";
    qApp->exit();
}

void Treeland::error()
{
    qCritical() << "Socket error: " << m_socket->errorString();
}

void Treeland::readyRead()
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
            qCDebug(dbus) << "Message received from daemon: Capabilities";
        } break;
        case DDM::DaemonMessages::LoginSucceeded:
        case DDM::DaemonMessages::UserActivateMessage: {
            QString user;
            input >> user;

            for (auto key : m_userWaylandSocket.keys()) {
                m_userWaylandSocket[key]->setEnabled(key == user);
            }

            struct passwd *pwd = getpwnam(user.toUtf8());
            m_helper->setCurrentUserId(pwd->pw_uid);
        } break;
        case DDM::DaemonMessages::SwitchToGreeter: {
            // Q_EMIT m_helper->greeterVisibleChanged();
        } break;
        default:
            break;
        }
    }
}

bool Treeland::ActivateWayland(QDBusUnixFileDescriptor _fd)
{
    if (!_fd.isValid()) {
        return false;
    }

    auto fd = std::make_shared<QDBusUnixFileDescriptor>();
    fd->swap(_fd);

    auto uid = connection().interface()->serviceUid(message().service());
    struct passwd *pw;
    pw = getpwuid(uid);
    QString user{ pw->pw_name };

    auto socket = std::make_shared<WSocket>(true);
    socket->create(fd->fileDescriptor(), false);

    socket->setEnabled(m_helper->currentUserId() == pw->pw_uid);

    m_helper->addSocket(socket.get());

    m_userWaylandSocket[user] = socket;
    m_userDisplayFds[user] = fd;

    connect(connection().interface(),
            &QDBusConnectionInterface::serviceUnregistered,
            socket.get(),
            [this, user] {
                m_userWaylandSocket.remove(user);
                m_userDisplayFds.remove(user);
            });

    return true;
}

QString Treeland::XWaylandName()
{
    setDelayedReply(true);

    auto uid = connection().interface()->serviceUid(message().service());
    struct passwd *pw;
    pw = getpwuid(uid);
    QString user{ pw->pw_name };

    auto *xwayland = m_helper->createXWayland();
    const QString &display = xwayland->displayName();

    auto m = message();
    auto conn = connection();

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished, [process, m, conn, user, display] {
        if (process->exitCode() != 0) {
            qCWarning(dbus) << "xhost command failed with exit code" << process->exitCode()
                            << process->readAllStandardOutput() << process->readAllStandardError();
            auto reply =
                m.createErrorReply(QDBusError::InternalError, "Failed to set xhost permissions");
            conn.send(reply);
        } else {
            qCDebug(dbus) << process->exitCode() << " " << process->readAllStandardOutput()
                          << process->readAllStandardError();
            qCDebug(dbus) << QString("user %1 got xwayland display %2.").arg(user).arg(display);
            auto reply = m.createReply(display);
            conn.send(reply);
        }
        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, [this, m, conn] {
        auto reply =
            m.createErrorReply(QDBusError::InternalError, "Failed to set xhost permissions");
        conn.send(reply);
    });

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("DISPLAY", display);
    process->setProcessEnvironment(env);
    process->setProgram("xhost");
    process->setArguments({ QString("+si:localuser:%1").arg(user) });
    process->start();

    return {};
}

} // namespace Treeland
