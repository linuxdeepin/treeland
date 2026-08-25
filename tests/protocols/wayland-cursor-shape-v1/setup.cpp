// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <winputdevice.h>
#include <wseat.h>

extern "C" {
#include <wlr/interfaces/wlr_pointer.h>
}

#include <cstdlib>

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    // Attach a real pointer device so the seat advertises pointer capability
    // and wl_seat_get_pointer yields a real wl_pointer resource for
    // cursor/gesture/relative managers.
    WSeat *seat = helper->seat();
    auto *pointer = (struct wlr_pointer *)calloc(1, sizeof(struct wlr_pointer));
    wlr_pointer_init(pointer, nullptr, "test-pointer");
    auto *device = new WInputDevice(&pointer->base, true);
    seat->attachInputDevice(device);
}
