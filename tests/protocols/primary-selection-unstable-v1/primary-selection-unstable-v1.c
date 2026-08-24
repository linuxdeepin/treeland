// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "primary-selection-unstable-v1-client-protocol.h"

#include <stdio.h>

struct ps_info {
    int data_offer_received;
    int selection_received;
};

static void handle_data_offer(void *data,
                              struct zwp_primary_selection_device_v1 *device,
                              struct zwp_primary_selection_offer_v1 *offer)
{
    (void)device;
    (void)offer;
    struct ps_info *info = data;
    info->data_offer_received = 1;
}

static void handle_selection(void *data,
                             struct zwp_primary_selection_device_v1 *device,
                             struct zwp_primary_selection_offer_v1 *offer)
{
    (void)device;
    (void)offer;
    struct ps_info *info = data;
    info->selection_received = 1;
}

static const struct zwp_primary_selection_device_v1_listener device_listener = {
    .data_offer = handle_data_offer,
    .selection = handle_selection,
};

static void handle_source_send(void *data,
                               struct zwp_primary_selection_source_v1 *source,
                               const char *mime_type, int32_t fd)
{
    (void)data;
    (void)source;
    (void)mime_type;
    (void)fd;
}

static void handle_source_cancelled(void *data,
                                    struct zwp_primary_selection_source_v1 *source)
{
    (void)data;
    (void)source;
}

static const struct zwp_primary_selection_source_v1_listener source_listener = {
    .send = handle_source_send,
    .cancelled = handle_source_cancelled,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    struct zwp_primary_selection_device_manager_v1 *manager =
        client_bind(&conn, "zwp_primary_selection_device_manager_v1",
                    &zwp_primary_selection_device_manager_v1_interface, 1);
    if (!seat || !manager) {
        fprintf(stderr, "primary-selection: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zwp_primary_selection_device_v1 *device =
        zwp_primary_selection_device_manager_v1_get_device(manager, seat);
    if (!device) {
        fprintf(stderr, "primary-selection: get_device returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ps_info info = {0};
    zwp_primary_selection_device_v1_add_listener(device, &device_listener, &info);

    /* Create a source and attempt to set the primary selection.  The headless
     * test seat has no input devices, so the serial is invalid and the request
     * is silently ignored — but no protocol error is raised.  This validates
     * the full protocol lifecycle: device creation, source creation, offering,
     * and the set_selection request path. */
    struct zwp_primary_selection_source_v1 *source =
        zwp_primary_selection_device_manager_v1_create_source(manager);
    if (!source) {
        fprintf(stderr, "primary-selection: create_source returned null\n");
        client_disconnect(&conn);
        return 1;
    }
    zwp_primary_selection_source_v1_add_listener(source, &source_listener, NULL);
    zwp_primary_selection_source_v1_offer(source, "text/plain");

    zwp_primary_selection_device_v1_set_selection(device, source, 0);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "primary-selection: set_selection raised an error\n");
        client_disconnect(&conn);
        return 1;
    }

    zwp_primary_selection_source_v1_destroy(source);
    zwp_primary_selection_device_v1_destroy(device);
    zwp_primary_selection_device_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
