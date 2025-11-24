// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
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
#include <QFile>
#include <QtEnvironmentVariables>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef QMap<QString, QString> StringMap;
Q_DECLARE_METATYPE(StringMap)

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

    auto active = [unixFileDescriptor, type](const QDBusConnection &connection) {
        auto activateFd = [unixFileDescriptor, type, connection] {
            QDBusInterface updateFd("org.deepin.Compositor1",
                                    "/org/deepin/Compositor1",
                                    "org.deepin.Compositor1",
                                    connection);

            if (updateFd.isValid()) {
                if (type == "wayland") {
                    updateFd.call("ActivateWayland", QVariant::fromValue(unixFileDescriptor));

                    QDBusInterface dbus("org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        QDBusConnection::sessionBus());
                    StringMap env;
                    env["WAYLAND_DISPLAY"] = "treeland.socket";
                    env["QT_QUICK_BACKEND"] = "software";

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

                    auto reply = dbus.call("UpdateActivationEnvironment", QVariant::fromValue(env));

                    sd_notify(0, "READY=1");
                } else if (type == "xwayland") {
                    QDBusReply<QString> reply = updateFd.call("XWaylandName");
                    if (reply.isValid()) {
                        const QString &xwaylandName = reply.value();

                        QDBusInterface dbus("org.freedesktop.DBus",
                                            "/org/freedesktop/DBus",
                                            "org.freedesktop.DBus",
                                            QDBusConnection::sessionBus());
                        StringMap env;
                        env["DISPLAY"] = xwaylandName;
                        env["QT_QUICK_BACKEND"] = "software";
                        auto reply =
                            dbus.call("UpdateActivationEnvironment", QVariant::fromValue(env));

                        sd_notify(0, "READY=1");
                    }
                }
            }
        };

        QDBusServiceWatcher *compositorWatcher =
            new QDBusServiceWatcher("org.deepin.Compositor1",
                                    connection,
                                    QDBusServiceWatcher::WatchForRegistration);

        QObject::connect(compositorWatcher,
                         &QDBusServiceWatcher::serviceRegistered,
                         compositorWatcher,
                         activateFd);

        activateFd();
    };

    active(QDBusConnection::sessionBus());
    active(QDBusConnection::systemBus());

    return app.exec();
}
