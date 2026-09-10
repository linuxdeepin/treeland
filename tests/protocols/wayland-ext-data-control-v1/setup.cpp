// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-ext-data-control-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <wseat.h>

#include <wlr/types/wlr_seat.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
wlr_seat *g_seat = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    auto *seat = helper->seat();
    if (seat)
        g_seat = seat->handle();
}

void ext_data_control_read_server_state(void *data)
{
    auto *state =
        static_cast<struct ext_data_control_server_state *>(data);
    state->valid = 0;
    state->has_source = 0;

    if (!g_seat)
        return;

    state->valid = 1;
    state->has_source = (g_seat->selection_source != nullptr) ? 1 : 0;
}
