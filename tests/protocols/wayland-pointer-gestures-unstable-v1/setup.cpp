// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-pointer-gestures-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <winputdevice.h>
#include <wseat.h>
#include <wserver.h>

extern "C" {
#include <wlr/interfaces/wlr_pointer.h>
}

#include <wlr/types/wlr_pointer_gestures_v1.h>

#include <wayland-server-core.h>

#include <cstdlib>
#include <cstring>

namespace {

/* Captured test client — set by the client_created listener. */
struct wl_client *g_client = nullptr;
struct wl_listener g_clientCreatedListener;

static void handleClientCreated(struct wl_listener *listener, void *data)
{
    (void)listener;
    g_client = static_cast<struct wl_client *>(data);
}

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

} // namespace

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    // Attach a real pointer device so the seat advertises pointer capability
    // and wl_seat_get_pointer yields a real wl_pointer resource for
    // cursor/gesture/relative managers.
    WSeat *seat = helper->seat();
    auto *pointer = static_cast<struct wlr_pointer *>(calloc(1, sizeof(struct wlr_pointer)));
    wlr_pointer_init(pointer, nullptr, "test-pointer");
    auto *device = new WInputDevice(&pointer->base, true);
    seat->attachInputDevice(device);

    // Register a client_created listener to capture the test client's
    // wl_client*.  Internal Wayland clients (if any) are created during
    // Treeland construction, before protocol_test_setup runs, so only the
    // test client triggers this callback.
    auto *server = server_for_helper(helper);
    if (server) {
        auto *display = server->handle();
        if (display) {
            g_clientCreatedListener.notify = handleClientCreated;
            wl_display_add_client_created_listener(display, &g_clientCreatedListener);
        }
    }
}

void pointer_gestures_read_server_state(void *data)
{
    auto *state = static_cast<struct pointer_gestures_server_state *>(data);
    state->valid = 0;
    state->swipes = 0;
    state->pinches = 0;

    if (!g_client)
        return;

    // E-level: locate the real wlr_pointer_gestures_v1 handle by iterating
    // the test client's bound resources.  When the client binds
    // zwp_pointer_gestures_v1, the resource's user_data is the
    // wlr_pointer_gestures_v1* (set by pointer_gestures_v1_bind in wlroots).
    struct find_gestures_data find = {};
    wl_client_for_each_resource(g_client, find_gestures_cb, &find);
    if (!find.gestures)
        return;

    state->valid = 1;

    // Read back the real production wl_list lengths.  When the client calls
    // get_swipe_gesture / get_pinch_gesture, wlroots inserts the gesture
    // resource into gestures->swipes / gestures->pinches respectively.
    state->swipes = wl_list_length(&find.gestures->swipes);
    state->pinches = wl_list_length(&find.gestures->pinches);
}
