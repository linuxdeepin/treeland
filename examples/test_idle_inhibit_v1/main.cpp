// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "idleclient.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Idle client test tool");
    parser.addHelpOption();

    QCommandLineOption idleTimeoutOption(QStringList() << "t" << "idle-timeout",
                                         "Set idle timeout in milliseconds",
                                         "timeout",
                                         "3000");
    parser.addOption(idleTimeoutOption);

    QCommandLineOption inhibitDurationOption(QStringList() << "i" << "inhibit-duration",
                                             "Set inhibit duration in milliseconds (0 to disable)",
                                             "duration",
                                             "0");
    parser.addOption(inhibitDurationOption);

    QCommandLineOption execOption(
        QStringList() << "e" << "exec",
        "Command to execute when idle (after inhibit duration if enabled)",
        "command");
    parser.addOption(execOption);

    QCommandLineOption dbusInhibitDurationOption(
        QStringList() << "d" << "dbus-inhibit-duration",
        "Set inhibit duration in milliseconds using org.freedesktop.ScreenSaver D-Bus interface (0 to disable). Conflict with -i option.",
        "duration",
        "0");
    parser.addOption(dbusInhibitDurationOption);

    parser.process(app);

    uint32_t idleTimeout = parser.value(idleTimeoutOption).toUInt();
    uint32_t inhibitDuration = parser.value(inhibitDurationOption).toUInt();
    uint32_t dbusInhibitDuration = parser.value(dbusInhibitDurationOption).toUInt();
    QString execCommand = parser.value(execOption);

    IdleClient client;
    if (!client.initialize(idleTimeout, inhibitDuration, dbusInhibitDuration, execCommand)) {
        qCritical("Failed to initialize idle client");
        return 1;
    }

    qInfo() << "Idle client started. Press Ctrl+C to exit.";
    qInfo() << "Monitoring idle state with timeout:" << idleTimeout << "ms";
    if (inhibitDuration > 0) {
        qInfo() << "Will inhibit idle for" << inhibitDuration << "ms when system goes idle";
    }
    if (!execCommand.isEmpty()) {
        qInfo() << "Will execute command on idle:" << execCommand;
    }

    return app.exec();
}
