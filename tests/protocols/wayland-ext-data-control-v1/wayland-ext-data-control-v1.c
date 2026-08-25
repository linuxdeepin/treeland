// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the ext_data_control_manager_v1 global served by Treeland
// (wlr_data_control_manager_v1_create in Helper).  On device creation wlroots
// emits the current clipboard state, so the client must observe `selection`
// and `primary_selection` events (NULL offers when no selection is set).

#include "client-connection.h"
#include "ext-data-control-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct dc_state {
    int selection_count;
    int primary_selection_count;
};

static void data_offer(void *data, struct ext_data_control_device_v1 *device,
                       struct ext_data_control_offer_v1 *offer)
{
    (void)data; (void)device; (void)offer;
}

static void selection(void *data, struct ext_data_control_device_v1 *device,
                      struct ext_data_control_offer_v1 *offer)
{
    (void)device;
    struct dc_state *state = data;
    state->selection_count++;
    if (offer)
        ext_data_control_offer_v1_destroy(offer);
}

static void primary_selection(void *data, struct ext_data_control_device_v1 *device,
                              struct ext_data_control_offer_v1 *offer)
{
    (void)device;
    struct dc_state *state = data;
    state->primary_selection_count++;
    if (offer)
        ext_data_control_offer_v1_destroy(offer);
}

static void finished(void *data, struct ext_data_control_device_v1 *device)
{
    (void)data; (void)device;
}

static const struct ext_data_control_device_v1_listener device_listener = {
    .data_offer = data_offer,
    .selection = selection,
    .finished = finished,
    .primary_selection = primary_selection,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "ext-data-control: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "ext-data-control: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_data_control_manager_v1 *manager = client_bind(
        &conn, "ext_data_control_manager_v1",
        &ext_data_control_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "ext-data-control: failed to bind manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct dc_state state;
    memset(&state, 0, sizeof(state));
    struct ext_data_control_device_v1 *device =
        ext_data_control_manager_v1_get_data_device(manager, seat);
    if (!device) {
        fprintf(stderr, "ext-data-control: get_data_device returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }
    ext_data_control_device_v1_add_listener(device, &device_listener, &state);

    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (state.selection_count < 1) {
        fprintf(stderr, "ext-data-control: no selection event received\n");
        failed = 1;
    }

    ext_data_control_device_v1_destroy(device);
    ext_data_control_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return failed;
}
