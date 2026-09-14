// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-cursor-shape-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <protocols/wcursorshapemanagerv1.h>

#include <wlr/types/wlr_cursor_shape_v1.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WCursorShapeManagerV1 *cursorShapeManager = nullptr;

struct CursorShapeRequestCapture {
    bool received;
    uint32_t shape;
    int deviceType;
    struct wl_listener listener;
} cursorShapeRequestCapture;

static void handleRequestSetShape(struct wl_listener *listener, void *data)
{
    (void)listener;
    auto *event = static_cast<
        wlr_cursor_shape_manager_v1_request_set_shape_event *>(data);
    cursorShapeRequestCapture.received = true;
    cursorShapeRequestCapture.shape = static_cast<uint32_t>(event->shape);
    cursorShapeRequestCapture.deviceType = static_cast<int>(event->device_type);
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    attach_test_pointer_device(helper);

    cursorShapeRequestCapture.received = false;
    cursorShapeRequestCapture.shape = 0;
    cursorShapeRequestCapture.deviceType = 0;
    cursorShapeRequestCapture.listener.notify = handleRequestSetShape;

    cursorShapeManager = find_server_interface<WCursorShapeManagerV1>(helper);
    if (cursorShapeManager && cursorShapeManager->handle()) {
        wl_signal_add(&cursorShapeManager->handle()->events.request_set_shape,
                      &cursorShapeRequestCapture.listener);
    }
}

void cursor_shape_read_server_state(void *data)
{
    auto *state = static_cast<struct cursor_shape_server_state *>(data);
    state->valid = cursorShapeRequestCapture.received ? 1 : 0;
    state->shape = cursorShapeRequestCapture.shape;
    state->device_type = cursorShapeRequestCapture.deviceType;
}
