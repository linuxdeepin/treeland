// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-security-context-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <protocols/wsecuritycontextmanager.h>

#include <wayland-server-core.h>
#include <cstddef>
#include <string.h>

WAYLIB_SERVER_USE_NAMESPACE

// Local copy of the production struct layout (defined in
// wsecuritycontextmanager.cpp).  Only the public fields we need are listed;
// the WLR_PRIVATE section is omitted since we don't access it.
struct local_mgr {
    struct wl_global *global;
    struct {
        struct wl_signal destroy;
        struct wl_signal commit;
        struct wl_signal new_client;
    } events;
};

struct local_state {
    char *sandbox_engine;
    char *app_id;
};

// Verify layout: sandbox_engine must be the first field so that app_id lands
// at the correct offset when casting from the production struct.
static_assert(offsetof(local_state, sandbox_engine) == 0, "layout mismatch");
static_assert(offsetof(local_state, app_id) == sizeof(char *), "layout mismatch");

struct local_commit_event {
    const struct local_state *state;
};

namespace {
bool g_committed = false;
bool g_app_id_match = false;

struct wl_listener g_commit_listener;

static void handleCommit(struct wl_listener *listener, void *data)
{
    (void)listener;
    const auto *event = static_cast<const struct local_commit_event *>(data);
    g_committed = true;
    if (event && event->state && event->state->app_id) {
        g_app_id_match = (strcmp(event->state->app_id, "test-app") == 0);
    }
}
}

// Accessor to call the protected global() method on WSecurityContextManager.
struct SecurityContextAccessor : WSecurityContextManager {
    using WSecurityContextManager::global;
};

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_committed = false;
    g_app_id_match = false;
    g_commit_listener.notify = handleCommit;

    auto *mgr = find_server_interface<WSecurityContextManager>(helper);
    if (mgr) {
        auto *g = static_cast<SecurityContextAccessor *>(mgr)->global();
        if (g) {
            auto *wlr_mgr = static_cast<struct local_mgr *>(
                wl_global_get_user_data(g));
            if (wlr_mgr) {
                wl_signal_add(&wlr_mgr->events.commit, &g_commit_listener);
            }
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
