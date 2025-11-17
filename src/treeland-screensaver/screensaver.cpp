// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

/**
 * org.freedesktop.ScreenSaver implementation using treeland_screensaver protocol
 * 
 * For each Inhibit request a wayland connection is created, which will be terminated
 * upon UnInhibit. The app will exit when there are no more active inhibits.
 */

#include <QCoreApplication>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QSocketNotifier>
#include "treeland-screensaver-v1.h"

// Global vars

static wl_display *display;
static QSocketNotifier *notifier;
static wl_registry *registry;
static uint32_t interfaceId;
static uint32_t interfaceVersion;

static uint cookieCounter = 0;
static QHash<QString, QHash<uint, treeland_screensaver *>> inhibits;

static QStringList callers;
QDBusServiceWatcher *watcher;

// Wayland global registry handling

static void handleGlobalRegister([[maybe_unused]] void        *data,
                                 [[maybe_unused]] wl_registry *reg,
                                 uint32_t                      id,
                                 const char                   *interface,
                                 uint32_t                      version) {
    if (strcmp(interface, "treeland_screensaver") == 0) {
        interfaceId = id;
        interfaceVersion = version;
    }
}

static void handleGlobalRemove([[maybe_unused]] void        *data,
                               [[maybe_unused]] wl_registry *registry,
                               uint32_t                      id) {
    if (id == interfaceId)
        QCoreApplication::instance()->quit();
}

static const wl_registry_listener registryListener = { 
    .global = handleGlobalRegister,
    .global_remove = handleGlobalRemove
};

// Inhibit / Uninhibit implementation

static treeland_screensaver *inhibit(const QString &appName, const QString &reason) {
    treeland_screensaver *screensaver = static_cast<treeland_screensaver *>(
        wl_registry_bind(registry, interfaceId, &treeland_screensaver_interface, interfaceVersion));
    treeland_screensaver_inhibit(screensaver,
                                 appName.toUtf8().constData(),
                                 reason.toUtf8().constData());
    return screensaver;
}

static void uninhibit(treeland_screensaver *screensaver) {
    treeland_screensaver_uninhibit(screensaver);
    treeland_screensaver_destroy(screensaver);
}

// D-Bus adaptor

class ScreenSaver : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")
public Q_SLOTS:
    uint Inhibit(const QString &appName, const QString &reason) {
        const QString caller = message().service();
        if (!callers.contains(caller)) {
            callers.append(caller);
            watcher->addWatchedService(caller);
        }
        if (!inhibits.contains(caller))
            inhibits.emplace(caller);
        inhibits[caller].insert(++cookieCounter, inhibit(appName, reason));
        return cookieCounter;
    }
    void UnInhibit(uint cookie) {
        const QString caller = message().service();
        if (inhibits.contains(caller) && inhibits[caller].contains(cookie)) {
            uninhibit(inhibits[caller][cookie]);
            inhibits[caller].remove(cookie);
            if (inhibits[caller].isEmpty())
                inhibits.remove(caller);
        }
        if (inhibits.isEmpty())
            QCoreApplication::quit();
    }
};

// D-Bus service watcher

#define DISCONNECTED(caller)                                                           \
    callers.removeAll(caller);                                                         \
    watcher->removeWatchedService(caller);                                             \
    if (inhibits.contains(caller)) {                                                   \
        for (auto it = inhibits[caller].cbegin(); it != inhibits[caller].cend(); ++it) \
            uninhibit(it.value());                                                     \
        inhibits.remove(caller);                                                       \
        if (inhibits.isEmpty())                                                        \
            QCoreApplication::quit();                                                  \
    }

static void onServiceUnregistered(const QString &service) {
    DISCONNECTED(service);
}

static void onServiceOwnerChanged(const QString &service,
                                  const QString &oldOwner,
                                  const QString &newOwner) {
    DISCONNECTED(service);
}

// Main function

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Connect to treeland and setup registry
    display = wl_display_connect(nullptr);
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, nullptr);
    wl_display_roundtrip(display);

    // Setup event loop
    notifier = new QSocketNotifier(wl_display_get_fd(display), QSocketNotifier::Read);
    QObject::connect(notifier, &QSocketNotifier::activated, [] {
        if (wl_display_dispatch(display) == -1)
            QCoreApplication::quit();

        if (wl_display_flush(display) == -1)
            QCoreApplication::quit();
    });

    // Setup watcher
    watcher = new QDBusServiceWatcher(&app);
    watcher->setConnection(QDBusConnection::sessionBus());
    watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(watcher,
                     &QDBusServiceWatcher::serviceUnregistered,
                     onServiceUnregistered);
    QObject::connect(watcher,
                     &QDBusServiceWatcher::serviceOwnerChanged,
                     onServiceOwnerChanged);

    // Register D-Bus service
    QDBusConnection::sessionBus().registerObject("/org/freedesktop/ScreenSaver",
                                                  new ScreenSaver(),
                                                  QDBusConnection::ExportAllSlots);
    QDBusConnection::sessionBus().registerService("org.freedesktop.ScreenSaver");

    // Let's rock & roll!!!
    return app.exec();
}

#include "screensaver.moc"
