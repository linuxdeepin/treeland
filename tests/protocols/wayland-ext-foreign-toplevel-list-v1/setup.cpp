// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-ext-foreign-toplevel-list-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <protocols/wextforeigntoplevellistv1.h>

#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WExtForeignToplevelListV1 *g_mgr = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_mgr = find_server_interface<WExtForeignToplevelListV1>(helper);
}

void foreign_toplevel_list_read_server_state(void *data)
{
    auto *state =
        static_cast<struct foreign_toplevel_list_server_state *>(data);
    state->valid = 0;
    state->count = 0;

    if (!g_mgr || !g_mgr->handle())
        return;

    state->valid = 1;
    state->count = wl_list_length(&g_mgr->handle()->toplevels);
}
