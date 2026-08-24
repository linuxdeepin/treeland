// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "ext-data-control-v1-client-protocol.h"

#include <stdio.h>

struct dc_info {
    int data_offer_received;
    int selection_received;
};

static void handle_data_offer(void *data,
                              struct ext_data_control_device_v1 *device,
                              struct ext_data_control_offer_v1 *offer)
{
    (void)device;
    (void)offer;
    struct dc_info *info = data;
    info->data_offer_received = 1;
}

static void handle_selection(void *data,
                             struct ext_data_control_device_v1 *device,
                             struct ext_data_control_offer_v1 *offer)
{
    (void)device;
    (void)offer;
    struct dc_info *info = data;
    info->selection_received = 1;
}

static void handle_primary_selection(void *data,
                                     struct ext_data_control_device_v1 *device,
                                     struct ext_data_control_offer_v1 *offer)
{
    (void)data;
    (void)device;
    (void)offer;
}

static void handle_finished(void *data,
                            struct ext_data_control_device_v1 *device)
{
    (void)data;
    (void)device;
}

static const struct ext_data_control_device_v1_listener device_listener = {
    .data_offer = handle_data_offer,
    .selection = handle_selection,
    .primary_selection = handle_primary_selection,
    .finished = handle_finished,
};

static void handle_source_send(void *data,
                               struct ext_data_control_source_v1 *source,
                               const char *mime_type, int32_t fd)
{
    (void)data;
    (void)source;
    (void)mime_type;
    (void)fd;
}

static void handle_source_cancelled(void *data,
                                    struct ext_data_control_source_v1 *source)
{
    (void)data;
    (void)source;
}

static const struct ext_data_control_source_v1_listener source_listener = {
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
    struct ext_data_control_manager_v1 *manager =
        client_bind(&conn, "ext_data_control_manager_v1",
                    &ext_data_control_manager_v1_interface, 1);
    if (!seat || !manager) {
        fprintf(stderr, "ext-data-control: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_data_control_device_v1 *device =
        ext_data_control_manager_v1_get_data_device(manager, seat);
    if (!device) {
        fprintf(stderr, "ext-data-control: get_data_device returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    struct dc_info info = {0};
    ext_data_control_device_v1_add_listener(device, &device_listener, &info);

    /* Create a data source, offer a MIME type, and set the selection.
     * ext-data-control is a privileged protocol: no serial is required. */
    struct ext_data_control_source_v1 *source =
        ext_data_control_manager_v1_create_data_source(manager);
    if (!source) {
        fprintf(stderr, "ext-data-control: create_data_source returned null\n");
        client_disconnect(&conn);
        return 1;
    }
    ext_data_control_source_v1_add_listener(source, &source_listener, NULL);
    ext_data_control_source_v1_offer(source, "text/plain");

    ext_data_control_device_v1_set_selection(device, source);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-data-control: roundtrip after set_selection failed\n");
        client_disconnect(&conn);
        return 1;
    }

    /* The compositor must deliver data_offer and selection events. */
    if (!info.data_offer_received || !info.selection_received) {
        fprintf(stderr, "ext-data-control: missing data_offer/selection events\n");
        client_disconnect(&conn);
        return 1;
    }

    ext_data_control_source_v1_destroy(source);
    ext_data_control_device_v1_destroy(device);
    ext_data_control_manager_v1_destroy(manager);
    client_disconnect(&conn);
    return 0;
}
