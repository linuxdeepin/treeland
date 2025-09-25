// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Pure C-style Wayland client for treeland_prelaunch_splash_manager_v1
// Usage: test-prelaunch-splash-client <app-id> [app-to-launch]

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include <QString>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>

#include <cstring>

#include <wayland-client.h>
#include <wayland-client-core.h>

#include "wayland-treeland-prelaunch-splash-v1-client-protocol.h"

struct ClientData {
    treeland_prelaunch_splash_manager_v1 *manager = nullptr;
};

static void handle_global(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    auto *c = static_cast<ClientData *>(data);
    if (strcmp(interface, "treeland_prelaunch_splash_manager_v1") == 0) {
        c->manager = static_cast<treeland_prelaunch_splash_manager_v1 *>(
            wl_registry_bind(registry, id, &treeland_prelaunch_splash_manager_v1_interface, 1));
        qInfo() << "Bound treeland_prelaunch_splash_manager_v1 version" << version;
    }
}

static void handle_global_remove(void *data, wl_registry *registry, uint32_t id)
{
    Q_UNUSED(data);
    Q_UNUSED(registry);
    Q_UNUSED(id);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qCritical() << "Usage:" << argv[0] << "<app-id> [command-to-launch]";
        qCritical() << "If only <app-id> is given, it'll also be used as the launch command (via dde-am).";
        return 1;
    }
    const char *appId = argv[1];
    // If only one argument is given, we also treat it as the launch target
    const char *launchApp = argc > 2 ? argv[2] : argv[1];

    wl_display *display = wl_display_connect(nullptr);
    if (!display) {
        qCritical() << "Cannot connect to Wayland display";
        return 1;
    }

    wl_registry *registry = wl_display_get_registry(display);
    ClientData data;
    static const wl_registry_listener regListener = { handle_global, handle_global_remove };
    wl_registry_add_listener(registry, &regListener, &data);
    wl_display_roundtrip(display); // receive globals

    if (!data.manager) {
        qWarning() << "treeland_prelaunch_splash_manager_v1 global not found";
    } else {
        treeland_prelaunch_splash_manager_v1_create_splash(data.manager, appId);
        qInfo() << "Sent create_splash for appId" << appId;
    if (launchApp && *launchApp) {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("dde-am", "dde-am", launchApp, (char*)nullptr);
                _exit(127);
            } else if (pid > 0) {
                qInfo() << "Requested launch via dde-am" << launchApp;
            } else {
                qWarning() << "fork() failed launching dde-am";
            }
        }
        wl_display_roundtrip(display); // flush request
    }

    QTimer::singleShot(200, [&]() {
        if (data.manager) {
            treeland_prelaunch_splash_manager_v1_destroy(data.manager);
        }
        wl_display_disconnect(display);
        app.quit();
    });

    return app.exec();
}
