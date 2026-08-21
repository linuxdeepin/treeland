// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/prelaunch-splash/prelaunchsplash.h"
#include "server-bridge.h"
#include "treeland-prelaunch-splash-v2.h"

#include <QString>
#include <wserver.h>

#include <wlr_all.h>

#include <cstdio>

WAYLIB_SERVER_USE_NAMESPACE

#ifndef DRM_FORMAT_ARGB8888
#define DRM_FORMAT_ARGB8888 0x34325241u
#endif
#ifndef DRM_FORMAT_XRGB8888
#define DRM_FORMAT_XRGB8888 0x34325258u
#endif

namespace {
splash_server_state g_state {};
}

static void recordRequest(const QString &appId, const QString &instanceId, bool iconNonNull)
{
    if (g_state.request_count < SPLASH_TEST_MAX_REQUESTS) {
        auto &record = g_state.requests[g_state.request_count];
        snprintf(record.app_id, sizeof(record.app_id), "%s", qPrintable(appId));
        snprintf(record.instance_id, sizeof(record.instance_id), "%s", qPrintable(instanceId));
        record.icon_non_null = iconNonNull ? 1 : 0;
    }
    ++g_state.request_count;
}

void protocol_test_setup(Helper *helper)
{
    auto *splash = find_server_interface<PrelaunchSplash>(helper);
    Q_ASSERT(splash);
    QObject::connect(splash, &PrelaunchSplash::splashRequested,
                     [](const QString &appId, const QString &instanceId, wlr_buffer *iconBuffer) {
                         recordRequest(appId, instanceId, iconBuffer != nullptr);
                     });
    QObject::connect(splash, &PrelaunchSplash::splashCloseRequested,
                     [](const QString &appId, const QString &instanceId) {
                         ++g_state.close_count;
                         snprintf(g_state.last_close_app_id, sizeof(g_state.last_close_app_id),
                                  "%s", qPrintable(appId));
                         snprintf(g_state.last_close_instance_id, sizeof(g_state.last_close_instance_id),
                                  "%s", qPrintable(instanceId));
                     });

}

extern "C" void splash_query_state(void *data)
{
    auto *state = static_cast<splash_server_state *>(data);
    *state = g_state;
}
