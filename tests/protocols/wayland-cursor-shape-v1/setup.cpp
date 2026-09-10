// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-cursor-shape-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <winputdevice.h>
#include <wseat.h>
#include <protocols/wcursorshapemanagerv1.h>

#include <wlr/types/wlr_cursor_shape_v1.h>

extern "C" {
#include <wlr/interfaces/wlr_pointer.h>
}

#include <cstdlib>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WCursorShapeManagerV1 *g_mgr = nullptr;

struct cursor_shape_listener {
    struct wl_listener listener;
    bool shape_requested;
    uint32_t captured_shape;
    int captured_device_type;
} g_state;

static void handleRequestSetShape(struct wl_listener *listener, void *data)
{
    (void)listener;
    auto *event = static_cast<
        wlr_cursor_shape_manager_v1_request_set_shape_event *>(data);
    g_state.shape_requested = true;
    g_state.captured_shape = static_cast<uint32_t>(event->shape);
    g_state.captured_device_type = static_cast<int>(event->device_type);
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    // Attach a real pointer device so the seat advertises pointer capability
    // and wl_seat_get_pointer yields a real wl_pointer resource for
    // cursor/gesture/relative managers.
    auto *seat = helper->seat();
    if (seat) {
        auto *pointer = static_cast<struct wlr_pointer *>(calloc(1, sizeof(struct wlr_pointer)));
        wlr_pointer_init(pointer, nullptr, "test-pointer");
        auto *inputDevice = new WInputDevice(&pointer->base, true);
        seat->attachInputDevice(inputDevice);
    }

    g_state.shape_requested = false;
    g_state.captured_shape = 0;
    g_state.captured_device_type = 0;
    g_state.listener.notify = handleRequestSetShape;

    g_mgr = find_server_interface<WCursorShapeManagerV1>(helper);
    if (g_mgr && g_mgr->handle()) {
        wl_signal_add(&g_mgr->handle()->events.request_set_shape,
                      &g_state.listener);
    }
}

void cursor_shape_read_server_state(void *data)
{
    auto *state = static_cast<struct cursor_shape_server_state *>(data);
    state->valid = g_state.shape_requested ? 1 : 0;
    state->shape = g_state.captured_shape;
    state->device_type = g_state.captured_device_type;
}
