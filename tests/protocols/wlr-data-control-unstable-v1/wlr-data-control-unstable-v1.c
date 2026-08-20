// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "wlr-data-control-unstable-v1-client-protocol.h"

#include <string.h>
#include <unistd.h>

struct source_state { unsigned send, cancelled, write_failed; const char *payload; };
struct device_state { struct zwlr_data_control_offer_v1 *offer; unsigned data_offer, selection, primary; };

static void source_send(void *data, struct zwlr_data_control_source_v1 *source, const char *mime, int32_t fd)
{
    (void)source;
    struct source_state *state = data;
    if (strcmp(mime, "text/plain") == 0) {
        const size_t payload_size = strlen(state->payload);
        if (write(fd, state->payload, payload_size) != (ssize_t)payload_size)
            ++state->write_failed;
    }
    close(fd);
    ++state->send;
}
static void source_cancelled(void *data, struct zwlr_data_control_source_v1 *source)
{ (void)source; ++((struct source_state *)data)->cancelled; }
static const struct zwlr_data_control_source_v1_listener source_listener = {
    .send = source_send, .cancelled = source_cancelled,
};
static void offer_offer(void *data, struct zwlr_data_control_offer_v1 *offer, const char *mime)
{ (void)data; (void)offer; (void)mime; }
static const struct zwlr_data_control_offer_v1_listener offer_listener = { .offer = offer_offer };
static void device_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
    (void)device;
    struct device_state *state = data;
    ++state->data_offer;
    state->offer = offer;
    zwlr_data_control_offer_v1_add_listener(offer, &offer_listener, state);
}
static void device_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{ (void)device; struct device_state *state = data; ++state->selection; if (offer) state->offer = offer; }
static void device_finished(void *data, struct zwlr_data_control_device_v1 *device)
{ (void)data; (void)device; }
static void device_primary(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{ (void)device; (void)offer; ++((struct device_state *)data)->primary; }
static const struct zwlr_data_control_device_v1_listener device_listener = {
    .data_offer = device_offer, .selection = device_selection,
    .finished = device_finished, .primary_selection = device_primary,
};

static int bind_device(struct client_connection *connection,
                       struct zwlr_data_control_manager_v1 **manager,
                       struct zwlr_data_control_device_v1 **device,
                       struct device_state *state)
{
    struct wl_seat *seat = client_bind(connection, "wl_seat", &wl_seat_interface, 1);
    *manager = client_bind(connection, "zwlr_data_control_manager_v1",
                                  &zwlr_data_control_manager_v1_interface, 2);
    if (!seat || !*manager) return 0;
    *device = zwlr_data_control_manager_v1_get_data_device(*manager, seat);
    wl_seat_destroy(seat);
    if (!*device) return 0;
    zwlr_data_control_device_v1_add_listener(*device, &device_listener, state);
    return wl_display_roundtrip(connection->display) >= 0;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection source_connection = {0}, target_connection = {0};
    struct zwlr_data_control_manager_v1 *source_manager = NULL, *target_manager = NULL;
    struct zwlr_data_control_device_v1 *source_device = NULL, *target_device = NULL;
    struct device_state source_device_state = {0}, target_state = {0};
    struct source_state first_state = { .payload = "treeland-data-control" };
    struct source_state second_state = { .payload = "replacement" };
    if (!client_connect(&source_connection, socket_name)
        || !client_connect(&target_connection, socket_name)
        || !bind_device(&source_connection, &source_manager, &source_device, &source_device_state)
        || !bind_device(&target_connection, &target_manager, &target_device, &target_state)) goto failed;

    struct zwlr_data_control_source_v1 *first = zwlr_data_control_manager_v1_create_data_source(source_manager);
    if (!first) goto failed;
    zwlr_data_control_source_v1_add_listener(first, &source_listener, &first_state);
    zwlr_data_control_source_v1_offer(first, "text/plain");
    zwlr_data_control_device_v1_set_selection(source_device, first);
    if (wl_display_roundtrip(source_connection.display) < 0 || wl_display_roundtrip(target_connection.display) < 0
        || !target_state.offer || !target_state.data_offer || target_state.selection < 2) goto failed;

    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) goto failed;
    zwlr_data_control_offer_v1_receive(target_state.offer, "text/plain", pipe_fds[1]);
    close(pipe_fds[1]);
    if (wl_display_roundtrip(target_connection.display) < 0 || wl_display_roundtrip(source_connection.display) < 0) { close(pipe_fds[0]); goto failed; }
    char payload[64] = {0};
    const ssize_t length = read(pipe_fds[0], payload, sizeof(payload) - 1);
    close(pipe_fds[0]);
    if (length != (ssize_t)strlen(first_state.payload) || strcmp(payload, first_state.payload) != 0
        || first_state.send != 1 || first_state.write_failed) goto failed;

    struct zwlr_data_control_source_v1 *second = zwlr_data_control_manager_v1_create_data_source(source_manager);
    if (!second) goto failed;
    zwlr_data_control_source_v1_add_listener(second, &source_listener, &second_state);
    zwlr_data_control_source_v1_offer(second, "text/plain");
    zwlr_data_control_device_v1_set_selection(source_device, second);
    if (wl_display_roundtrip(source_connection.display) < 0 || first_state.cancelled != 1) goto failed;

    struct zwlr_data_control_source_v1 *third = zwlr_data_control_manager_v1_create_data_source(source_manager);
    if (!third) goto failed;
    zwlr_data_control_source_v1_add_listener(third, &source_listener, &second_state);
    zwlr_data_control_source_v1_offer(third, "text/plain");
    zwlr_data_control_device_v1_set_primary_selection(source_device, third);
    if (wl_display_roundtrip(source_connection.display) < 0 || wl_display_roundtrip(target_connection.display) < 0
        || target_state.primary < 2) goto failed;

    // XML keeps manager-created children valid after manager.destroy.
    zwlr_data_control_manager_v1_destroy(source_manager);
    source_manager = NULL;
    zwlr_data_control_device_v1_set_selection(source_device, NULL);
    if (wl_display_roundtrip(source_connection.display) < 0) goto failed;

    zwlr_data_control_device_v1_destroy(target_device);
    zwlr_data_control_manager_v1_destroy(target_manager);
    zwlr_data_control_device_v1_destroy(source_device);
    client_disconnect(&target_connection);
    client_disconnect(&source_connection);
    return 0;
failed:
    client_disconnect(&target_connection);
    client_disconnect(&source_connection);
    return 1;
}
