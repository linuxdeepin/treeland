// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-idle-inhibit-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <wserver.h>
#include <wsurface.h>

#include <QtCore/qglobal.h>

#include <wlr_all.h>

#include <cstring>
#include <unistd.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
struct wl_display *g_display = nullptr;

struct find_inhibitor_data {
    struct wlr_idle_inhibitor_v1 *inhibitor = nullptr;
};

enum wl_iterator_result find_inhibitor(struct wl_resource *resource, void *data)
{
    auto *find = static_cast<struct find_inhibitor_data *>(data);
    const char *name = wl_resource_get_class(resource);
    if (!name || strcmp(name, "zwp_idle_inhibitor_v1") != 0)
        return WL_ITERATOR_CONTINUE;

    find->inhibitor = static_cast<struct wlr_idle_inhibitor_v1 *>(
        wl_resource_get_user_data(resource));
    return WL_ITERATOR_STOP;
}

struct wl_client *find_test_client_with_inhibitor(struct find_inhibitor_data *find)
{
    struct wl_client *client;
    wl_client_for_each(client, wl_display_get_client_list(g_display)) {
        pid_t pid = -1;
        wl_client_get_credentials(client, &pid, nullptr, nullptr);
        if (pid != getpid())
            continue;

        wl_client_for_each_resource(client, find_inhibitor, find);
        if (find->inhibitor)
            return client;
    }
    return nullptr;
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    auto *server = server_for_helper(helper);
    Q_ASSERT(server);
    g_display = server->handle();
    Q_ASSERT(g_display);
}

void idle_inhibit_read_server_state(void *data)
{
    auto *state = static_cast<struct idle_inhibit_server_state *>(data);
    state->valid = 0;
    state->surface_mapped = 0;

    if (!g_display)
        return;

    struct find_inhibitor_data find;
    if (!find_test_client_with_inhibitor(&find))
        return;
    if (!find.inhibitor || !find.inhibitor->surface)
        return;

    state->valid = 1;
    auto *surface = WSurface::fromHandle(find.inhibitor->surface);
    state->surface_mapped = surface && surface->mapped();
}
