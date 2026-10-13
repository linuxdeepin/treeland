// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-relative-pointer-unstable-v1.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include <wbackend.h>
#include <protocols/wrelativepointermanagerv1.h>

#include <wlr/types/wlr_relative_pointer_v1.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WRelativePointerManagerV1 *g_mgr = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    attach_test_pointer_device(server);

    g_mgr = find_server_interface<WRelativePointerManagerV1>(server);
}

void relative_pointer_read_server_state(void *data)
{
    auto *state = static_cast<struct relative_pointer_server_state *>(data);
    state->valid = 0;
    state->count = 0;

    if (!g_mgr || !g_mgr->handle())
        return;

    state->valid = 1;
    state->count = wl_list_length(&g_mgr->handle()->relative_pointers);
}
