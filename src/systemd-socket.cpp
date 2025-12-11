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
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QtEnvironmentVariables>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef QMap<QString, QString> StringMap;
Q_DECLARE_METATYPE(StringMap)

class SignalReceiver : public QObject
{
    Q_OBJECT
public:
    SignalReceiver(std::function<void()> activateFdFunc, QObject *parent = nullptr)
        : QObject(parent), activateFd(activateFdFunc) {
    }
public Q_SLOTS:
    void onSessionChanged() {
        activateFd();
    }
private:
    std::function<void()> activateFd;
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

    QList<QTemporaryFile *> tmpFiles;
    QDBusUnixFileDescriptor unixFileDescriptor(SD_LISTEN_FDS_START);

    auto active = [unixFileDescriptor, type, &tmpFiles](QDBusConnection connection) {
        auto activateFd = [unixFileDescriptor, type, &connection, &tmpFiles] {
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
                    QDBusMessage reply = updateFd.call("XWaylandName");
                    if (reply.type() == QDBusMessage::ReplyMessage) {
                        QVariantList values = reply.arguments();
                        if (values.size() < 2) {
                            qWarning() << "Invalid XWaylandName reply";
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
                            qWarning() << "Failed to create temporary xauth file";
                            return;
                        }
                        QString authFileName = authFile->fileName();
                        tmpFiles.append(authFile);
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
                        auto reply =
                            dbus.call("UpdateActivationEnvironment", QVariant::fromValue(env));

                        sd_notify(0, "READY=1");
                    }
                }
            }
        };

        connection.connect("org.deepin.Compositor1",
                           "/org/deepin/Compositor1",
                           "org.deepin.Compositor1",
                           "SessionChanged",
                           new SignalReceiver(activateFd),
                           SLOT(onSessionChanged()));
        activateFd();
    };

    active(QDBusConnection::sessionBus());
    active(QDBusConnection::systemBus());

    int ret = app.exec();
    for (auto i : tmpFiles)
        delete i;
    return ret;
}

#include "systemd-socket.moc"
