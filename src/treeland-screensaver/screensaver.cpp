// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

/**
 * org.freedesktop.ScreenSaver implementation using treeland_screensaver protocol
 * 
 * For each Inhibit request a wayland connection is created, which will be terminated
 * upon UnInhibit. The app will exit when there are no more active inhibits.
 */

#include <QCoreApplication>
#include <QDBusInterface>
#include <QSocketNotifier>
#include "treeland-screensaver-v1.h"

struct Connection {
    wl_display *display;
    QSocketNotifier *notifier;
    treeland_screensaver *screensaver;
};

// Wayland global registry handling

static void handleGlobalRegister(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    auto screensaver = static_cast<void **>(data);
    if (strcmp(interface, "treeland_screensaver") == 0)
        *screensaver = wl_registry_bind(registry, name, &treeland_screensaver_interface, version);
}

static void handleGlobalRemove([[maybe_unused]] void *data, [[maybe_unused]] wl_registry *registry, [[maybe_unused]] uint32_t name) {}

static const wl_registry_listener registryListener = { 
    .global = handleGlobalRegister,
    .global_remove = handleGlobalRemove
};

// Inhibit / Uninhibit implementation

#define PREPARE_READ(display) \
    while (wl_display_prepare_read(display) != 0) \
        wl_display_dispatch_pending(display); \
    wl_display_flush(display);

static Connection inhibit(const QString &appName, const QString &reason) {
    auto display = wl_display_connect(nullptr);
    auto registry = wl_display_get_registry(display);
    treeland_screensaver *screensaver = nullptr;

    wl_registry_add_listener(registry, &registryListener, &screensaver);
    wl_display_roundtrip(display);

    if (!screensaver) {
        wl_display_disconnect(display);
        throw std::runtime_error("Failed to bind to treeland_screensaver interface");
    }

    treeland_screensaver_inhibit(screensaver, appName.toUtf8().constData(), reason.toUtf8().constData());

    PREPARE_READ(display);
    auto notifier = new QSocketNotifier(wl_display_get_fd(display), QSocketNotifier::Read);
    QObject::connect(notifier, &QSocketNotifier::activated, [display] {
        wl_display_read_events(display);
        PREPARE_READ(display);
    });

    return Connection{display, notifier, screensaver};
}

static void uninhibit(Connection &conn) {
    treeland_screensaver_uninhibit(conn.screensaver);
    treeland_screensaver_destroy(conn.screensaver);
    wl_display_flush(conn.display);
    wl_display_disconnect(conn.display);
    QObject::disconnect(conn.notifier, &QSocketNotifier::activated, nullptr, nullptr);
    delete conn.notifier;
}

// Global vars

static uint cookieCounter = 0;
static QHash<uint, Connection> inhibits;

// D-Bus adaptor

class ScreenSaver : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")
public Q_SLOTS:
    uint Inhibit(const QString &appName, const QString &reason) {
        inhibits.insert(++cookieCounter, inhibit(appName, reason));
        return cookieCounter;
    }
    void UnInhibit(uint cookie) {
        if (inhibits.contains(cookie)) {
            uninhibit(inhibits[cookie]);
            inhibits.remove(cookie);
        }
        if (inhibits.isEmpty())
            QCoreApplication::instance()->quit();
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QDBusConnection::sessionBus().registerObject("/org/freedesktop/ScreenSaver",
                                                  new ScreenSaver(),
                                                  QDBusConnection::ExportAllSlots);
    QDBusConnection::sessionBus().registerService("org.freedesktop.ScreenSaver");

    return app.exec();
}

#include "screensaver.moc"
