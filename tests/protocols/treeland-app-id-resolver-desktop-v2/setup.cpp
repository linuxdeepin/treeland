// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "treeland-app-id-resolver-desktop-v2.h"
#include "workspace/workspace.h"

#include "treelandconfig.hpp"

#include <QEventLoop>
#include <QPointer>
#include <wbackend.h>

namespace {
constexpr auto TestAppId = "org.deepin.treeland.protocol.app-id-resolver";

QPointer<SurfaceWrapper> g_splashWrapper;
bool g_splashConfigReady = false;

bool isTestSplash(const SurfaceWrapper *wrapper)
{
    return wrapper && wrapper->type() == SurfaceWrapper::Type::SplashScreen
        && wrapper->appId() == QLatin1String(TestAppId);
}

void enablePrelaunchSplash(WaylandServer *server)
{
    server->globalConfig()->setEnablePrelaunchSplash(true);
    g_splashConfigReady = true;
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    auto *config = server->globalConfig();
    if (config->isInitializeSucceeded()) {
        enablePrelaunchSplash(server);
    } else {
        QObject::connect(config,
                         &TreelandConfig::configInitializeSucceed,
                         server,
                         [server](auto *) { enablePrelaunchSplash(server); });
    }

    QObject::connect(server->workspace(),
                     &SurfaceContainer::surfaceAdded,
                     server,
                     [](SurfaceWrapper *wrapper) {
                         if (isTestSplash(wrapper))
                             g_splashWrapper = wrapper;
                     });
}

extern "C" bool protocol_test_ready(WaylandServer *server)
{
    return g_splashConfigReady && !server->rootSurfaceContainer()->outputs().isEmpty();
}

extern "C" void app_id_resolver_desktop_wait_for_splash(void *data)
{
    auto *created = static_cast<int *>(data);
    *created = g_splashWrapper ? 1 : 0;
    if (*created)
        return;

    QEventLoop eventLoop;
    QObject::connect(WaylandServer::instance()->workspace(),
                     &SurfaceContainer::surfaceAdded,
                     &eventLoop,
                     [&eventLoop, created](SurfaceWrapper *wrapper) {
                         if (isTestSplash(wrapper)) {
                             *created = 1;
                             eventLoop.quit();
                         }
                     });
    eventLoop.exec();
}

extern "C" void app_id_resolver_desktop_read_state(void *data)
{
    app_id_resolver_desktop_state state {};
    auto *server = WaylandServer::instance();
    const auto surfaces = server->workspace()->surfaces();
    state.output_ready = !server->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    state.workspace_surface_count = surfaces.size();
    state.splash_created = g_splashWrapper ? 1 : 0;
    if (g_splashWrapper) {
        state.wrapper_in_workspace = surfaces.contains(g_splashWrapper) ? 1 : 0;
        state.wrapper_converted_to_xdg =
            g_splashWrapper->type() == SurfaceWrapper::Type::XdgToplevel ? 1 : 0;
        state.wrapper_app_id_matches =
            g_splashWrapper->appId() == QLatin1String(TestAppId) ? 1 : 0;
    }
    *static_cast<app_id_resolver_desktop_state *>(data) = state;
}
