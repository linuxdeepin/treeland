// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <systemd/sd-daemon.h>

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QFile>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusUnixFileDescriptor>
#include <QtEnvironmentVariables>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (sd_listen_fds(0) > 1) {
        fprintf(stderr, "Too many file descriptors received.\n");
        exit(1);
    }

    QDBusUnixFileDescriptor unixFileDescriptor(SD_LISTEN_FDS_START);

    auto active = [unixFileDescriptor](const QDBusConnection &connection) {
        auto activateFd = [unixFileDescriptor, connection] {
            QDBusInterface updateFd("org.deepin.Compositor1",
                                    "/org/deepin/Compositor1",
                                    "org.deepin.Compositor1",
                                    connection);

            if (updateFd.isValid()) {
                updateFd.call("Activate", QVariant::fromValue(unixFileDescriptor));
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

    sd_notify(0, "READY=1");

    return app.exec();
}
