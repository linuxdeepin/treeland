// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-pointer-gestures-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <wserver.h>

#include <wlr/types/wlr_pointer_gestures_v1.h>

#include <wayland-server-core.h>

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

/* The test client runs in the protocol-test process; Xwayland does not. */
struct wl_display *g_display = nullptr;

/* Callback data for wl_client_for_each_resource. */
struct find_gestures_data {
    struct wlr_pointer_gestures_v1 *gestures;
};

static enum wl_iterator_result find_gestures_cb(struct wl_resource *resource,
                                                void *user_data)
{
    auto *data = static_cast<struct find_gestures_data *>(user_data);
    const char *name = wl_resource_get_class(resource);
    if (name && strcmp(name, "zwp_pointer_gestures_v1") == 0) {
        data->gestures = static_cast<struct wlr_pointer_gestures_v1 *>(
            wl_resource_get_user_data(resource));
        return WL_ITERATOR_STOP;
    }
    return WL_ITERATOR_CONTINUE;
}

static struct wl_client *find_test_client_with_gestures(
    struct find_gestures_data *find)
{
    struct wl_client *client;
    wl_client_for_each(client, wl_display_get_client_list(g_display)) {
        pid_t pid = -1;
        wl_client_get_credentials(client, &pid, nullptr, nullptr);
        if (pid != getpid())
            continue;

        wl_client_for_each_resource(client, find_gestures_cb, find);
        if (find->gestures)
            return client;
    }
    return nullptr;
}

static int count_resources_for_client(struct wl_list *resources,
                                      struct wl_client *client)
{
    int count = 0;
    struct wl_resource *resource;
    wl_resource_for_each(resource, resources) {
        if (wl_resource_get_client(resource) == client)
            ++count;
    }
    return count;
}

} // namespace

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    attach_test_pointer_device(helper);

    auto *server = server_for_helper(helper);
    Q_ASSERT(server);
    g_display = server ? server->handle() : nullptr;
    Q_ASSERT(g_display);
}

void pointer_gestures_read_server_state(void *data)
{
    auto *state = static_cast<struct pointer_gestures_server_state *>(data);
    state->valid = 0;
    state->swipes = 0;
    state->pinches = 0;

    if (!g_display)
        return;

    // Locate the test client's wl_client by credentials and by the manager
    // resource it bound, rather than by connection order.  Multiple internal
    // clients share the protocol-test PID.  When the test client binds
    // zwp_pointer_gestures_v1, the resource's user_data is the
    // wlr_pointer_gestures_v1* (set by pointer_gestures_v1_bind in wlroots).
    struct find_gestures_data find = {};
    auto *test_client = find_test_client_with_gestures(&find);
    if (!test_client)
        return;

    state->valid = 1;

    // Count only resources owned by this test client.  wlroots stores all
    // clients' gestures in shared lists, so global list lengths could pass
    // because an unrelated client (for example Xwayland) created gestures.
    state->swipes = count_resources_for_client(&find.gestures->swipes, test_client);
    state->pinches = count_resources_for_client(&find.gestures->pinches, test_client);
}
