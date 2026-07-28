// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/wallpaper-color/wallpapercolorinterfacev1.h"
#include "modules/window-management/windowmanagementinterfacev1.h"
#include "modules/virtual-output/virtualoutputmanagerinterfacev1.h"
#include "modules/prelaunch-splash/prelaunchsplash.h"

#include <wserver.h>
#include <wsocket.h>

#include <qwdisplay.h>

#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <QThread>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QCoreApplication>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <poll.h>

#include "wayland-treeland-wallpaper-color-v1-client-protocol.h"
#include "wayland-treeland-window-management-v1-client-protocol.h"
#include "wayland-treeland-virtual-output-manager-v1-client-protocol.h"
#include "wayland-treeland-prelaunch-splash-v2-client-protocol.h"

WAYLIB_SERVER_USE_NAMESPACE

static const int CLIENT_TIMEOUT_MS = 5000;

// Non-blocking Wayland dispatch with poll timeout
static bool dispatchWaylandEvents(wl_display *display, int timeoutMs = 100)
{
    wl_display_flush(display);
    struct pollfd pfd;
    pfd.fd = wl_display_get_fd(display);
    pfd.events = POLLIN;
    pfd.revents = 0;
    int ret = poll(&pfd, 1, timeoutMs);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        wl_display_dispatch(display);
        return true;
    }
    wl_display_dispatch_pending(display);
    return false;
}

class ProtocolIntegrationTest : public QObject
{
    Q_OBJECT

    WServer *m_server = nullptr;
    WSocket *m_socket = nullptr;
    WallpaperColorInterfaceV1 *m_wallpaperColor = nullptr;
    WindowManagementInterfaceV1 *m_windowManagement = nullptr;
    VirtualOutputManagerInterfaceV1 *m_virtualOutput = nullptr;
    PrelaunchSplash *m_prelaunchSplash = nullptr;

    QTemporaryDir m_tmpDir;
    int m_sv[2] = {-1, -1};

    bool setupServer()
    {
        m_server = new WServer();

        m_wallpaperColor = m_server->attach<WallpaperColorInterfaceV1>(m_server);
        m_windowManagement = m_server->attach<WindowManagementInterfaceV1>(m_server);
        m_virtualOutput = m_server->attach<VirtualOutputManagerInterfaceV1>(m_server);
        m_prelaunchSplash = m_server->attach<PrelaunchSplash>();

        if (!m_wallpaperColor || !m_windowManagement || !m_virtualOutput || !m_prelaunchSplash) {
            return false;
        }

        m_tmpDir.setAutoRemove(true);
        QString socketPath = m_tmpDir.path() + "/wayland-test-0";
        m_socket = new WSocket(true, m_server);
        if (!m_socket->create(socketPath)) {
            return false;
        }
        m_server->addSocket(m_socket);
        m_server->start();

        return true;
    }

    void cleanupServer()
    {
        if (m_sv[1] >= 0) {
            close(m_sv[1]);
            m_sv[1] = -1;
        }
        if (m_sv[0] >= 0) {
            close(m_sv[0]);
            m_sv[0] = -1;
        }
        delete m_server;
        m_server = nullptr;
        m_socket = nullptr;
        m_wallpaperColor = nullptr;
        m_windowManagement = nullptr;
        m_virtualOutput = nullptr;
        m_prelaunchSplash = nullptr;
    }

    wl_display *connectClient()
    {
        // Create a fresh socketpair for each client connection
        // wl_display_disconnect closes the fd, so we cannot reuse m_sv[1]
        if (m_sv[0] >= 0) {
            close(m_sv[0]);
            m_sv[0] = -1;
        }
        if (m_sv[1] >= 0) {
            close(m_sv[1]);
            m_sv[1] = -1;
        }
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_sv) != 0) {
            return nullptr;
        }
        m_socket->addClient(m_sv[0]);
        return wl_display_connect_to_fd(m_sv[1]);
    }

    static void syncCallback(void *data, wl_callback *callback, uint32_t)
    {
        *static_cast<bool *>(data) = true;
        wl_callback_destroy(callback);
    }

    static const wl_callback_listener syncListener;

    bool waitForSync(wl_display *display, int timeoutMs = CLIENT_TIMEOUT_MS)
    {
        bool done = false;
        wl_callback *callback = wl_display_sync(display);
        wl_callback_add_listener(callback, &syncListener, &done);
        QElapsedTimer timer;
        timer.start();
        while (!done && timer.elapsed() < timeoutMs) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        return done;
    }

private Q_SLOTS:

    void initTestCase()
    {
        if (!qEnvironmentVariableIsEmpty("CI")) {
            QSKIP("test_protocol_integration requires a full Wayland compositor "
                  "environment and does not work in headless CI. Run locally with "
                  "ctest -R test_protocol_integration.");
        }
        QVERIFY(setupServer());
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        cleanupServer();
    }

    void testWallpaperColorWatch()
    {
        m_wallpaperColor->updateWallpaperColor("HDMI-1", true);

        wl_display *display = connectClient();
        QVERIFY(display != nullptr);

        struct Context {
            wl_registry *registry = nullptr;
            bool gotOutputColor = false;
            QString outputName;
            uint32_t isDark = 0;
            treeland_wallpaper_color_manager_v1 *manager = nullptr;
        } ctx;

        static const wl_registry_listener registryListener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
                auto *c = static_cast<Context *>(data);
                if (strcmp(iface, "treeland_wallpaper_color_manager_v1") == 0) {
                    c->manager = static_cast<treeland_wallpaper_color_manager_v1 *>(
                        wl_registry_bind(registry, name, &treeland_wallpaper_color_manager_v1_interface, 1));
                }
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        auto registry = wl_display_get_registry(display);
        ctx.registry = registry;
        wl_registry_add_listener(registry, &registryListener, &ctx);
        QVERIFY(waitForSync(display));
        QVERIFY(ctx.manager != nullptr);

        static const treeland_wallpaper_color_manager_v1_listener listener = {
            [](void *data, treeland_wallpaper_color_manager_v1 *, const char *output, uint32_t isdark) {
                auto *c = static_cast<Context *>(data);
                c->gotOutputColor = true;
                c->outputName = QString::fromUtf8(output);
                c->isDark = isdark;
            }
        };
        treeland_wallpaper_color_manager_v1_add_listener(ctx.manager, &listener, &ctx);
        treeland_wallpaper_color_manager_v1_watch(ctx.manager, "HDMI-1");
        wl_display_flush(display);
        QCoreApplication::processEvents();

        QElapsedTimer timer;
        timer.start();
        while (!ctx.gotOutputColor && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }

        QVERIFY(ctx.gotOutputColor);
        QCOMPARE(ctx.outputName, QString("HDMI-1"));
        QCOMPARE(ctx.isDark, 1u);

        treeland_wallpaper_color_manager_v1_destroy(ctx.manager);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
    }

    void testWallpaperColorUpdate()
    {
        m_wallpaperColor->updateWallpaperColor("DP-1", false);

        wl_display *display = connectClient();
        QVERIFY(display != nullptr);

        struct Context {
            int eventCount = 0;
            uint32_t lastIsDark = 0;
            treeland_wallpaper_color_manager_v1 *manager = nullptr;
        } ctx;

        static const wl_registry_listener registryListener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
                auto *c = static_cast<Context *>(data);
                if (strcmp(iface, "treeland_wallpaper_color_manager_v1") == 0) {
                    c->manager = static_cast<treeland_wallpaper_color_manager_v1 *>(
                        wl_registry_bind(registry, name, &treeland_wallpaper_color_manager_v1_interface, 1));
                }
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        auto registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registryListener, &ctx);
        QVERIFY(waitForSync(display));
        QVERIFY(ctx.manager != nullptr);

        static const treeland_wallpaper_color_manager_v1_listener listener = {
            [](void *data, treeland_wallpaper_color_manager_v1 *, const char *, uint32_t isdark) {
                auto *c = static_cast<Context *>(data);
                c->eventCount++;
                c->lastIsDark = isdark;
            }
        };
        treeland_wallpaper_color_manager_v1_add_listener(ctx.manager, &listener, &ctx);
        treeland_wallpaper_color_manager_v1_watch(ctx.manager, "DP-1");
        wl_display_flush(display);
        QCoreApplication::processEvents();

        QElapsedTimer timer;
        timer.start();
        while (ctx.eventCount < 1 && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        QCOMPARE(ctx.eventCount, 1);
        QCOMPARE(ctx.lastIsDark, 0u);

        m_wallpaperColor->updateWallpaperColor("DP-1", true);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        timer.restart();
        while (ctx.eventCount < 2 && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        QCOMPARE(ctx.eventCount, 2);
        QCOMPARE(ctx.lastIsDark, 1u);

        treeland_wallpaper_color_manager_v1_destroy(ctx.manager);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
    }

    void testWindowManagementSetDesktop()
    {
        wl_display *display = connectClient();
        QVERIFY(display != nullptr);

        struct Context {
            bool gotShowDesktop = false;
            uint32_t state = 0;
            treeland_window_management_v1 *manager = nullptr;
        } ctx;

        static const wl_registry_listener registryListener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
                auto *c = static_cast<Context *>(data);
                if (strcmp(iface, "treeland_window_management_v1") == 0) {
                    c->manager = static_cast<treeland_window_management_v1 *>(
                        wl_registry_bind(registry, name, &treeland_window_management_v1_interface, 1));
                }
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        auto registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registryListener, &ctx);
        QVERIFY(waitForSync(display));
        QVERIFY(ctx.manager != nullptr);

        static const treeland_window_management_v1_listener listener = {
            [](void *data, treeland_window_management_v1 *, uint32_t state) {
                auto *c = static_cast<Context *>(data);
                c->gotShowDesktop = true;
                c->state = state;
            }
        };
        treeland_window_management_v1_add_listener(ctx.manager, &listener, &ctx);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        QElapsedTimer timer;
        timer.start();
        while (!ctx.gotShowDesktop && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        QVERIFY(ctx.gotShowDesktop);
        QCOMPARE(ctx.state, 0u);

        treeland_window_management_v1_set_desktop(ctx.manager, 1);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        timer.restart();
        while (ctx.state != 1u && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }

        treeland_window_management_v1_destroy(ctx.manager);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
    }

    void testWindowManagementServerPush()
    {
        wl_display *display = connectClient();
        QVERIFY(display != nullptr);

        struct Context {
            uint32_t state = 99;
            treeland_window_management_v1 *manager = nullptr;
        } ctx;

        static const wl_registry_listener registryListener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
                auto *c = static_cast<Context *>(data);
                if (strcmp(iface, "treeland_window_management_v1") == 0) {
                    c->manager = static_cast<treeland_window_management_v1 *>(
                        wl_registry_bind(registry, name, &treeland_window_management_v1_interface, 1));
                }
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        auto registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registryListener, &ctx);
        QVERIFY(waitForSync(display));
        QVERIFY(ctx.manager != nullptr);

        static const treeland_window_management_v1_listener listener = {
            [](void *data, treeland_window_management_v1 *, uint32_t state) {
                static_cast<Context *>(data)->state = state;
            }
        };
        treeland_window_management_v1_add_listener(ctx.manager, &listener, &ctx);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        QElapsedTimer timer;
        timer.start();
        while (ctx.state == 99 && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        QCOMPARE(ctx.state, 0u);

        m_windowManagement->setDesktopState(WindowManagementInterfaceV1::DesktopState::Show);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        timer.restart();
        while (ctx.state != 1u && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        QCOMPARE(ctx.state, 1u);

        treeland_window_management_v1_destroy(ctx.manager);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
    }

    void testVirtualOutputCreateAndList()
    {
        wl_display *display = connectClient();
        QVERIFY(display != nullptr);

        struct Context {
            bool gotList = false;
            treeland_virtual_output_manager_v1 *manager = nullptr;
            QByteArray listData;
        } ctx;

        static const wl_registry_listener registryListener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
                auto *c = static_cast<Context *>(data);
                if (strcmp(iface, "treeland_virtual_output_manager_v1") == 0) {
                    c->manager = static_cast<treeland_virtual_output_manager_v1 *>(
                        wl_registry_bind(registry, name, &treeland_virtual_output_manager_v1_interface, 1));
                }
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        auto registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registryListener, &ctx);
        QVERIFY(waitForSync(display));
        QVERIFY(ctx.manager != nullptr);

        static const treeland_virtual_output_manager_v1_listener managerListener = {
            [](void *data, treeland_virtual_output_manager_v1 *, wl_array *names) {
                auto *c = static_cast<Context *>(data);
                c->gotList = true;
                c->listData = QByteArray(static_cast<const char *>(names->data), names->size);
            }
        };
        treeland_virtual_output_manager_v1_add_listener(ctx.manager, &managerListener, &ctx);

        treeland_virtual_output_manager_v1_get_virtual_output_list(ctx.manager);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        QElapsedTimer timer;
        timer.start();
        while (!ctx.gotList && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }
        QVERIFY(ctx.gotList);

        treeland_virtual_output_manager_v1_destroy(ctx.manager);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
    }

    void testPrelaunchSplashCreate()
    {
        wl_display *display = connectClient();
        QVERIFY(display != nullptr);

        QSignalSpy spy(m_prelaunchSplash, &PrelaunchSplash::splashRequested);

        struct Context {
            treeland_prelaunch_splash_manager_v2 *manager = nullptr;
        } ctx;

        static const wl_registry_listener registryListener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
                auto *c = static_cast<Context *>(data);
                if (strcmp(iface, "treeland_prelaunch_splash_manager_v2") == 0) {
                    c->manager = static_cast<treeland_prelaunch_splash_manager_v2 *>(
                        wl_registry_bind(registry, name, &treeland_prelaunch_splash_manager_v2_interface, 1));
                }
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        auto registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registryListener, &ctx);
        QVERIFY(waitForSync(display));
        QVERIFY(ctx.manager != nullptr);

        treeland_prelaunch_splash_manager_v2_create_splash(ctx.manager, "com.test.app", "instance1", "flatpak", nullptr);
        wl_display_flush(display);
        QCoreApplication::processEvents();

        QElapsedTimer timer;
        while (spy.count() < 1 && timer.elapsed() < CLIENT_TIMEOUT_MS) {
            dispatchWaylandEvents(display, 100);
            QCoreApplication::processEvents();
        }

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("com.test.app"));
        QCOMPARE(spy.at(0).at(1).toString(), QString("instance1"));

        treeland_prelaunch_splash_manager_v2_destroy(ctx.manager);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
    }
};

const wl_callback_listener ProtocolIntegrationTest::syncListener = {
    ProtocolIntegrationTest::syncCallback
};

QTEST_MAIN(ProtocolIntegrationTest)
#include "main.moc"
