// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-security-context-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <protocols/wsecuritycontextmanager.h>

#include <wlr/types/wlr_security_context_v1.h>
#include <wayland-server-core.h>
#include <string.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
bool g_committed = false;
bool g_app_id_match = false;

struct wl_listener g_commit_listener;

static void handleCommit(struct wl_listener *listener, void *data)
{
    (void)listener;
    const auto *event = static_cast<const struct wlr_security_context_v1_commit_event *>(data);
    g_committed = true;
    if (event && event->state && event->state->app_id) {
        g_app_id_match = (strcmp(event->state->app_id, "test-app") == 0);
    }
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_committed = false;
    g_app_id_match = false;
    g_commit_listener.notify = handleCommit;

    auto *mgr = find_server_interface<WSecurityContextManager>(helper);
    if (mgr) {
        auto *wlr_mgr = static_cast<struct wlr_security_context_manager_v1 *>(mgr->handle());
        if (wlr_mgr) {
            wl_signal_add(&wlr_mgr->events.commit, &g_commit_listener);
        }
    }
}

void security_context_read_server_state(void *data)
{
    auto *state =
        static_cast<struct security_context_server_state *>(data);
    state->valid = g_committed ? 1 : 0;
    state->app_id_match = g_app_id_match ? 1 : 0;
}
