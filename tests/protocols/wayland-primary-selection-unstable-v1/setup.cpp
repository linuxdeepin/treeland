// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-primary-selection-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <wseat.h>
#include <winputdevice.h>

#include <wlr/types/wlr_seat.h>

extern "C" {
#include <wlr/interfaces/wlr_keyboard.h>
}

#include <cstdlib>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
wlr_seat *g_seat = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    // Attach a keyboard device so the seat advertises keyboard capability.
    // When a toplevel is mapped, Treeland gives it keyboard focus, which
    // sends a wl_keyboard::enter event with a valid serial. The client
    // captures this serial and uses it for set_primary_selection.
    auto *seat = helper->seat();
    if (seat) {
        g_seat = seat->handle();
        auto *keyboard = (struct wlr_keyboard *)calloc(1, sizeof(struct wlr_keyboard));
        wlr_keyboard_init(keyboard, nullptr, "test-keyboard");
        auto *device = new WInputDevice(&keyboard->base, true);
        seat->attachInputDevice(device);
    }
}

void primary_selection_read_server_state(void *data)
{
    auto *state =
        static_cast<struct primary_selection_server_state *>(data);
    state->valid = 0;
    state->has_source = 0;

    if (!g_seat)
        return;

    state->valid = 1;
    state->has_source = (g_seat->primary_selection_source != nullptr) ? 1 : 0;
}
