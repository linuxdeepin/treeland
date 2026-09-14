// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-relative-pointer-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <winputdevice.h>
#include <wseat.h>
#include <protocols/wrelativepointermanagerv1.h>

#include <wlr/types/wlr_relative_pointer_v1.h>

extern "C" {
#include <wlr/interfaces/wlr_pointer.h>
}

#include <cstdlib>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WRelativePointerManagerV1 *g_mgr = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    // Attach a pointer device so wl_seat advertises pointer capability.
    auto *seat = helper->seat();
    if (seat) {
        auto *pointer = static_cast<struct wlr_pointer *>(calloc(1, sizeof(struct wlr_pointer)));
        wlr_pointer_init(pointer, nullptr, "test-pointer");
        auto *inputDevice = new WInputDevice(&pointer->base, true);
        seat->attachInputDevice(inputDevice);
    }

    g_mgr = find_server_interface<WRelativePointerManagerV1>(helper);
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
