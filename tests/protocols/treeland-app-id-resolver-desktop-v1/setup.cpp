// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "treeland-app-id-resolver-desktop-v1.h"
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

void enablePrelaunchSplash(Helper *helper)
{
    helper->globalConfig()->setEnablePrelaunchSplash(true);
    g_splashConfigReady = true;
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    auto *config = helper->globalConfig();
    if (config->isInitializeSucceeded()) {
        enablePrelaunchSplash(helper);
    } else {
        QObject::connect(config,
                         &TreelandConfig::configInitializeSucceed,
                         helper,
                         [helper](auto *) { enablePrelaunchSplash(helper); });
    }

    QObject::connect(helper->workspace(),
                     &SurfaceContainer::surfaceAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (isTestSplash(wrapper))
                             g_splashWrapper = wrapper;
                     });
}

extern "C" bool protocol_test_ready(Helper *helper)
{
    return g_splashConfigReady && !helper->rootSurfaceContainer()->outputs().isEmpty();
}

extern "C" void app_id_resolver_desktop_wait_for_splash(void *data)
{
    auto *created = static_cast<int *>(data);
    *created = g_splashWrapper ? 1 : 0;
    if (*created)
        return;

    QEventLoop eventLoop;
    QObject::connect(Helper::instance()->workspace(),
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
    auto *helper = Helper::instance();
    const auto surfaces = helper->workspace()->surfaces();
    state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
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
