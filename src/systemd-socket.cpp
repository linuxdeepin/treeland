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
#include <QSaveFile>
#include <QTimer>
#include <QtEnvironmentVariables>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <memory>
#include <optional>
#include <utility>

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
        watchService(connection);

        QDBusInterface test("org.deepin.Compositor1",
                            "/org/deepin/Compositor1",
                            "org.deepin.Compositor1",
                            connection);
        if (!test.isValid())
            return false;

        if (m_type == "xwayland") {
            m_compositorBus = busFromConnection(connection);
            return true;
        }

        return connectActivationSignal(connection);
    }

    void start() {
        if (tryStart(QDBusConnection::sessionBus()) || tryStart(QDBusConnection::systemBus())) {
            m_started = true;
            clearPendingRetry(StartRetry);
            activate();
            return;
        }
        scheduleStartRetry();
    }

public Q_SLOTS:
    void activate() {
        if (!m_started)
            return;

        if (!m_compositorBus.has_value())
            return;

        auto connection = dbusConnection(*m_compositorBus);

        QDBusInterface updateFd("org.deepin.Compositor1",
                                "/org/deepin/Compositor1",
                                "org.deepin.Compositor1",
                                connection);

        if (updateFd.isValid()) {
            if (m_type == "wayland") {
                if (!callDBus(updateFd,
                              QStringLiteral("ActivateWayland"),
                              QStringLiteral("Failed to activate Wayland socket"),
                              QVariant::fromValue(*m_unixFileDescriptor))) {
                    return;
                }

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

                if (!callDBus(dbus,
                              QStringLiteral("UpdateActivationEnvironment"),
                              QStringLiteral("Failed to update Wayland activation environment"),
                              QVariant::fromValue(env))) {
                    return;
                }

                sd_notify(0, "READY=1");
            } else if (m_type == "xwayland") {
                QDBusMessage reply = updateFd.call("XWaylandName");
                if (reply.type() == QDBusMessage::ReplyMessage) {
                    QVariantList values = reply.arguments();
                    if (values.size() < 2) {
                        qCWarning(lcSdSocket) << "Invalid XWaylandName reply";
                        scheduleActivateRetry();
                        return;
                    }
                    QString xwaylandName = values.at(0).toString();
                    QByteArray auth = values.at(1).toByteArray();

                    if (m_lastXwaylandAuth == auth) {
                        sd_notify(0, "READY=1");
                        clearPendingRetry(ActivateRetry);
                        return;
                    }

                    const QString authFileName = xauthorityFileName();
                    if (!writeXAuthority(authFileName, auth)) {
                        scheduleActivateRetry();
                        return;
                    }

                    QDBusInterface dbus("org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        QDBusConnection::sessionBus());
                    StringMap env;
                    env["DISPLAY"] = xwaylandName;
                    env["XAUTHORITY"] = authFileName;
                    if (!callDBus(dbus,
                                  QStringLiteral("UpdateActivationEnvironment"),
                                  QStringLiteral("Failed to update XWayland activation environment"),
                                  QVariant::fromValue(env))) {
                        scheduleActivateRetry();
                        return;
                    }

                    sd_notify(0, "READY=1");
                    m_lastXwaylandAuth = auth;
                    clearPendingRetry(ActivateRetry);
                } else {
                    scheduleActivateRetry();
                }
            }
        }
        if (m_type == "xwayland" && !updateFd.isValid()) {
            scheduleActivateRetry();
        }
    }

private:
    enum class Bus {
        Session,
        System,
    };

    enum RetryFlag {
        StartRetry = 1 << 0,
        ActivateRetry = 1 << 1,
    };

    bool isRetryPending(RetryFlag flag) const {
        return m_pendingRetries & flag;
    }

    void setPendingRetry(RetryFlag flag) {
        m_pendingRetries |= flag;
    }

    void clearPendingRetry(RetryFlag flag) {
        m_pendingRetries &= ~flag;
    }

    Bus busFromConnection(const QDBusConnection &connection) const {
        return connection.name() == QDBusConnection::sessionBus().name()
            ? Bus::Session
            : Bus::System;
    }

    QDBusConnection dbusConnection(Bus bus) const {
        return bus == Bus::Session
            ? QDBusConnection::sessionBus()
            : QDBusConnection::systemBus();
    }

    QString runtimeFileName(const QString &fileName) const {
        QByteArray runtimeDir = qgetenv("XDG_RUNTIME_DIR");
        if (runtimeDir.isEmpty())
            runtimeDir = QDir::tempPath().toLocal8Bit();

        return QStringLiteral("%1/%2").arg(QString::fromLocal8Bit(runtimeDir), fileName);
    }

    QString xauthorityFileName() const {
        return runtimeFileName(QStringLiteral("treeland-xauthority"));
    }

    bool writeXAuthority(const QString &fileName, const QByteArray &auth) const {
        QSaveFile file(fileName);
        if (!file.open(QFile::WriteOnly)) {
            qCWarning(lcSdSocket) << "Failed to open xauth file" << fileName << file.errorString();
            return false;
        }

        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        if (file.write(auth) != auth.size()) {
            qCWarning(lcSdSocket) << "Failed to write xauth file" << fileName << file.errorString();
            return false;
        }

        if (!file.commit()) {
            qCWarning(lcSdSocket) << "Failed to commit xauth file" << fileName << file.errorString();
            return false;
        }

        return true;
    }

    template<typename... Args>
    std::optional<QDBusMessage> callDBus(QDBusInterface &interface,
                                         const QString &method,
                                         const QString &errorMessage,
                                         Args &&...args) {
        QDBusMessage reply = interface.call(method, std::forward<Args>(args)...);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(lcSdSocket) << errorMessage << reply.errorMessage();
            return std::nullopt;
        }

        return reply;
    }

    bool connectActivationSignal(QDBusConnection connection) {
        const auto bus = busFromConnection(connection);
        if (m_compositorBus == bus)
            return true;

        if (m_compositorBus.has_value()) {
            auto oldConnection = dbusConnection(*m_compositorBus);
            oldConnection.disconnect("org.deepin.Compositor1",
                                     "/org/deepin/Compositor1",
                                     "org.deepin.Compositor1",
                                     QStringLiteral("SessionChanged"),
                                     this,
                                     SLOT(activate()));
            m_compositorBus.reset();
        }

        if (connection.connect("org.deepin.Compositor1",
                               "/org/deepin/Compositor1",
                               "org.deepin.Compositor1",
                               QStringLiteral("SessionChanged"),
                               this,
                               SLOT(activate()))) {
            m_compositorBus = bus;
            return true;
        }

        return false;
    }

    void watchService(const QDBusConnection &connection) {
        if (findChild<QDBusServiceWatcher *>(connection.name()))
            return;

        const auto bus = busFromConnection(connection);
        auto *watcher = new QDBusServiceWatcher(QStringLiteral("org.deepin.Compositor1"),
                                                connection,
                                                QDBusServiceWatcher::WatchForRegistration
                                                    | QDBusServiceWatcher::WatchForUnregistration,
                                                this);
        watcher->setObjectName(connection.name());
        connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this, bus](const QString &service) {
            Q_UNUSED(service)
            if (!m_started || m_compositorBus == bus) {
                start();
            }
        });
        connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this, bus](const QString &service) {
            Q_UNUSED(service)
            if (m_compositorBus == bus) {
                m_started = false;
                resetXwaylandActivationState();
                scheduleStartRetry();
            }
        });
    }

    void scheduleStartRetry() {
        if (isRetryPending(StartRetry))
            return;

        setPendingRetry(StartRetry);
        QTimer::singleShot(RetryIntervalMs, this, [this] {
            if (!isRetryPending(StartRetry))
                return;

            clearPendingRetry(StartRetry);
            start();
        });
    }

    void scheduleActivateRetry() {
        if (m_type != "xwayland" || isRetryPending(ActivateRetry))
            return;

        setPendingRetry(ActivateRetry);
        QTimer::singleShot(RetryIntervalMs, this, [this] {
            if (!isRetryPending(ActivateRetry))
                return;

            clearPendingRetry(ActivateRetry);
            activate();
        });
    }

    void resetXwaylandActivationState() {
        if (m_type != "xwayland")
            return;

        m_lastXwaylandAuth.clear();
        clearPendingRetry(ActivateRetry);
    }

    static constexpr int RetryIntervalMs = 500;

    std::shared_ptr<QDBusUnixFileDescriptor> m_unixFileDescriptor;
    QString m_type;
    bool m_started = false;
    int m_pendingRetries = 0;
    QByteArray m_lastXwaylandAuth;
    std::optional<Bus> m_compositorBus;
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
