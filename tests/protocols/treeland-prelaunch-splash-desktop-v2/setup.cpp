// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-prelaunch-splash-desktop-v2.h"
#include "workspace/workspace.h"

#include "treelandconfig.hpp"

#include <wbackend.h>

#include <QPointer>
#include <QEventLoop>

namespace {
constexpr auto SplashAppId = "org.deepin.treeland.protocol.splash";

QPointer<SurfaceWrapper> g_wrapper;
prelaunch_splash_desktop_state g_state {};
bool g_splashConfigReady = false;

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
                     [helper](SurfaceWrapper *wrapper) {
                         if (!wrapper || wrapper->type() != SurfaceWrapper::Type::SplashScreen
                             || wrapper->appId() != QLatin1String(SplashAppId)) {
                             return;
                         }

                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace =
                             helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                         g_state.wrapper_is_splash = 1;
                         g_state.wrapper_has_qml_item = wrapper->prelaunchSplash() ? 1 : 0;
                         g_state.wrapper_width = qRound(wrapper->implicitWidth());
                         g_state.wrapper_height = qRound(wrapper->implicitHeight());
                         qstrncpy(g_state.app_id, qPrintable(wrapper->appId()), sizeof(g_state.app_id));

                         QObject::connect(wrapper, &QObject::destroyed, helper, [] {
                             g_state.wrapper_destroyed = 1;
                         });
                     });
}

extern "C" bool protocol_test_ready(Helper *helper)
{
    return g_splashConfigReady && !helper->rootSurfaceContainer()->outputs().isEmpty();
}

extern "C" void prelaunch_splash_desktop_wait_for_creation(void *data)
{
    auto *created = static_cast<int *>(data);
    *created = g_wrapper ? 1 : 0;
    if (*created)
        return;

    QEventLoop eventLoop;
    QObject::connect(Helper::instance()->workspace(),
                     &SurfaceContainer::surfaceAdded,
                     &eventLoop,
                     [&eventLoop, created](SurfaceWrapper *wrapper) {
                         if (wrapper && wrapper->type() == SurfaceWrapper::Type::SplashScreen
                             && wrapper->appId() == QLatin1String(SplashAppId)) {
                             *created = 1;
                             eventLoop.quit();
                         }
                     });
    eventLoop.exec();
}

extern "C" void prelaunch_splash_desktop_wait_for_destruction(void *data)
{
    auto *destroyed = static_cast<int *>(data);
    *destroyed = g_state.wrapper_destroyed;
    if (*destroyed)
        return;

    QEventLoop eventLoop;
    QObject::connect(g_wrapper, &QObject::destroyed, &eventLoop, [&eventLoop, destroyed] {
        *destroyed = 1;
        eventLoop.quit();
    });
    eventLoop.exec();
}

extern "C" void prelaunch_splash_desktop_read_state(void *data)
{
    auto state = g_state;
    auto *helper = Helper::instance();
    state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (g_wrapper) {
        state.wrapper_in_workspace = helper->workspace()->surfaces().contains(g_wrapper) ? 1 : 0;
        state.wrapper_is_splash = g_wrapper->type() == SurfaceWrapper::Type::SplashScreen ? 1 : 0;
        state.wrapper_has_qml_item = g_wrapper->prelaunchSplash() ? 1 : 0;
        state.wrapper_width = qRound(g_wrapper->implicitWidth());
        state.wrapper_height = qRound(g_wrapper->implicitHeight());
        qstrncpy(state.app_id, qPrintable(g_wrapper->appId()), sizeof(state.app_id));
    } else if (state.wrapper_created) {
        state.wrapper_in_workspace = 0;
    }
    *static_cast<prelaunch_splash_desktop_state *>(data) = state;
}
