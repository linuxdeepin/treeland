#include <qdbusconnection.h>
#include <qdbusinterface.h>
#include <qdbusunixfiledescriptor.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QFile>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (sd_listen_fds(0) > 1) {
        fprintf(stderr, "Too many file descriptors received.\n");
        exit(1);
    }

    QDBusUnixFileDescriptor unixFileDescriptor(SD_LISTEN_FDS_START);

    auto activateFd = [unixFileDescriptor] {
    // FIXME: current only support session level
#if 1
        QDBusInterface updateFd(
            "org.deepin.Compositor1", "/org/deepin/Compositor1",
            "org.deepin.Compositor1", QDBusConnection::sessionBus());
#else
        QDBusInterface updateFd(
            "org.deepin.Compositor1", "/org/deepin/Compositor1",
            "org.deepin.Compositor1", QDBusConnection::systemBus());
#endif
        updateFd.call("Activate", QVariant::fromValue(unixFileDescriptor));
    };

    QDBusServiceWatcher *compositorWatcher = new QDBusServiceWatcher(
        "org.deepin.Compositor1", QDBusConnection::systemBus(),
        QDBusServiceWatcher::WatchForRegistration);

    QObject::connect(compositorWatcher, &QDBusServiceWatcher::serviceRegistered,
                     compositorWatcher, activateFd);

    activateFd();

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(
        "org.deepin.dde.Session1", QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(watcher, &QDBusServiceWatcher::serviceUnregistered, [&] {
        qInfo() << "dde session exit";
        QDBusInterface systemd("org.freedesktop.systemd1",
                               "/org/freedesktop/systemd1",
                               "org.freedesktop.systemd1.Manager");
        systemd.call("StartUnit", "dde-fake-session-shutdown.target",
                     "replace");
        qApp->quit();
    });

    sd_notify(0, "READY=1");

    return app.exec();
}
