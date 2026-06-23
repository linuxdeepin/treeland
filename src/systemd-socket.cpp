// Copyright (C) 2023-2026 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <systemd/sd-daemon.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QTimer>
#include <QTemporaryFile>
#include <QtEnvironmentVariables>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

Q_DECLARE_LOGGING_CATEGORY(lcSdSocket)
Q_LOGGING_CATEGORY(lcSdSocket, "treeland.systemd.socket")

typedef QMap<QString, QString> StringMap;
Q_DECLARE_METATYPE(StringMap)

class SocketActivator : public QObject
{
    Q_OBJECT
public:
    SocketActivator(QDBusUnixFileDescriptor unixFileDescriptor,
                     QString type,
                     QObject *parent = nullptr)
        : QObject(parent)
        , m_unixFileDescriptor(std::make_shared<QDBusUnixFileDescriptor>(std::move(unixFileDescriptor)))
        , m_type(std::move(type))
        , m_started(false) {
    }

    bool tryStart(const QDBusConnection &connection) {
        QDBusInterface test("org.deepin.Compositor1",
                            "/org/deepin/Compositor1",
                            "org.deepin.Compositor1",
                            connection);
        if (!test.isValid())
            return false;

        m_busName = connection.name();
        const QString signal = m_type == "xwayland" ? "XWaylandReady" : "SessionChanged";
        qCWarning(lcSdSocket) << "start: using bus" << m_busName << "signal:" << signal;
        if (m_busName == "session")
            QDBusConnection::sessionBus().connect("org.deepin.Compositor1",
                                                  "/org/deepin/Compositor1",
                                                  "org.deepin.Compositor1",
                                                  signal,
                                                  this,
                                                  SLOT(activate()));
        else
            QDBusConnection::systemBus().connect("org.deepin.Compositor1",
                                                 "/org/deepin/Compositor1",
                                                 "org.deepin.Compositor1",
                                                 signal,
                                                 this,
                                                 SLOT(activate()));
        return true;
    }

    void start() {
        if (tryStart(QDBusConnection::sessionBus()) || tryStart(QDBusConnection::systemBus())) {
            m_started = true;
            activate();
            return;
        }
        qCCritical(lcSdSocket) << "org.deepin.Compositor1 not found on session or system bus";
    }

    ~SocketActivator() {
        qDeleteAll(m_tmpFiles);
    }

public Q_SLOTS:
    void activate() {
        if (!m_started)
            return;

        auto connection = m_busName == "session"
            ? QDBusConnection::sessionBus()
            : QDBusConnection::systemBus();

        QDBusInterface updateFd("org.deepin.Compositor1",
                                "/org/deepin/Compositor1",
                                "org.deepin.Compositor1",
                                connection);

        qCWarning(lcSdSocket) << "activate:" << m_type << "updateFd.isValid() =" << updateFd.isValid() << "on bus" << m_busName;
        if (updateFd.isValid()) {
            if (m_type == "wayland") {
                updateFd.call("ActivateWayland", QVariant::fromValue(*m_unixFileDescriptor));

                QDBusInterface dbus("org.freedesktop.DBus",
                                    "/org/freedesktop/DBus",
                                    "org.freedesktop.DBus",
                                    QDBusConnection::sessionBus());
                StringMap env;
                env["WAYLAND_DISPLAY"] = "treeland.socket";

                const auto extraEnvs = qgetenv("TREELAND_SESSION_ENVIRONMENTS");
                if (!extraEnvs.isEmpty()) {
                    const auto envs = extraEnvs.split('\n');
                    for (const auto &i : envs) {
                        const auto pair = i.split('=');
                        if (pair.size() == 2) {
                            env[QString::fromLocal8Bit(pair[0])] = QString::fromLocal8Bit(pair[1]);
                        }
                    }
                }

                dbus.call("UpdateActivationEnvironment", QVariant::fromValue(env));

                sd_notify(0, "READY=1");
            } else if (m_type == "xwayland") {
                QDBusMessage reply = updateFd.call("XWaylandName");
                qCWarning(lcSdSocket) << "XWaylandName reply type:" << reply.type() << "error:" << reply.errorMessage();
                if (reply.type() == QDBusMessage::ReplyMessage) {
                    QVariantList values = reply.arguments();
                    if (values.size() < 2) {
                        qCWarning(lcSdSocket) << "Invalid XWaylandName reply";
                        return;
                    }
                    QString xwaylandName = values.at(0).toString();
                    QByteArray auth = values.at(1).toByteArray();

                    QByteArray runtimeDir = qgetenv("XDG_RUNTIME_DIR");
                    if (runtimeDir.isEmpty())
                        runtimeDir = QDir::tempPath().toLocal8Bit();
                    QTemporaryFile *authFile = new QTemporaryFile();
                    authFile->setFileTemplate(QStringLiteral("%1/.xauth_XXXXXX").arg(runtimeDir));
                    if (!authFile->open()) {
                        qCWarning(lcSdSocket) << "Failed to create temporary xauth file";
                        return;
                    }
                    QString authFileName = authFile->fileName();
                    m_tmpFiles.append(authFile);
                    authFile->setPermissions(QFile::ReadOwner | QFile::WriteOwner);
                    authFile->write(auth);
                    authFile->flush();
                    authFile->close();

                    QDBusInterface dbus("org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        QDBusConnection::sessionBus());
                    StringMap env;
                    env["DISPLAY"] = xwaylandName;
                    env["XAUTHORITY"] = authFileName;
                    dbus.call("UpdateActivationEnvironment", QVariant::fromValue(env));

                    sd_notify(0, "READY=1");
                    qCWarning(lcSdSocket) << "XWayland ready: DISPLAY=" << xwaylandName << "XAUTHORITY=" << authFileName;
                } else if (reply.type() == QDBusMessage::ErrorMessage) {
                    qCWarning(lcSdSocket) << "XWaylandName failed:" << reply.errorMessage();
                }
            }
        }
        if (m_type == "xwayland" && !updateFd.isValid()) {
            if (++m_retryCount < 20) {
                qCWarning(lcSdSocket) << "XWayland D-Bus not ready, retry" << m_retryCount << "/20";
                QTimer::singleShot(500, this, &SocketActivator::activate);
            } else {
                qCWarning(lcSdSocket) << "XWayland activation timed out after 10s";
            }
        }
    }

private:
    std::shared_ptr<QDBusUnixFileDescriptor> m_unixFileDescriptor;
    QString m_type;
    QString m_busName;
    bool m_started = false;
    QList<QTemporaryFile *> m_tmpFiles;
    int m_retryCount = 0;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDBusRegisterMetaType<StringMap>();

    QCommandLineParser parser;
    parser.setApplicationDescription("Treeland socket helper");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption typeOption(QStringList() << "t"
                                                << "type",
                                  "xwayland",
                                  "wayland");
    parser.addOption(typeOption);

    parser.process(app);

    const QString &type = parser.value(typeOption);

    if (type == "wayland" && sd_listen_fds(0) > 1) {
        fprintf(stderr, "Too many file descriptors received.\n");
        exit(1);
    }

    QDBusUnixFileDescriptor unixFileDescriptor(SD_LISTEN_FDS_START);

    auto *activator = new SocketActivator(unixFileDescriptor, type, &app);
    activator->start();

    return app.exec();
}

#include "systemd-socket.moc"
