// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wayland-data-device.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "wlr-data-control-unstable-v1-client-protocol.h"

#include <stdlib.h>
#include <unistd.h>

struct source_state {
    const char *payload;
    unsigned sends;
    unsigned cancelled;
    int write_failed;
};

struct selection_offer {
    struct selection_offer *next;
    struct wl_data_offer *proxy;
    int has_text;
};

struct target_state {
    struct selection_offer *offers;
    struct wl_data_offer *selection;
};

static void source_send(void *data, struct zwlr_data_control_source_v1 *source,
                        const char *mime, int32_t fd)
{
    (void)source;
    struct source_state *state = data;
    const size_t size = strlen(state->payload);
    if (strcmp(mime, "text/plain") != 0
        || write(fd, state->payload, size) != (ssize_t)size)
        state->write_failed = 1;
    ++state->sends;
    close(fd);
}

static void source_cancelled(void *data, struct zwlr_data_control_source_v1 *source)
{
    (void)source;
    ++((struct source_state *)data)->cancelled;
}

static const struct zwlr_data_control_source_v1_listener source_listener = {
    .send = source_send,
    .cancelled = source_cancelled,
};

static void control_offer_mime(void *data, struct zwlr_data_control_offer_v1 *offer,
                               const char *mime)
{
    (void)data;
    (void)offer;
    (void)mime;
}

static const struct zwlr_data_control_offer_v1_listener control_offer_listener = {
    .offer = control_offer_mime,
};

static void control_offer(void *data, struct zwlr_data_control_device_v1 *device,
                          struct zwlr_data_control_offer_v1 *offer)
{
    (void)data;
    (void)device;
    zwlr_data_control_offer_v1_add_listener(offer, &control_offer_listener, NULL);
}

static void control_selection(void *data, struct zwlr_data_control_device_v1 *device,
                              struct zwlr_data_control_offer_v1 *offer)
{
    (void)data;
    (void)device;
    if (offer)
        zwlr_data_control_offer_v1_destroy(offer);
}

static void control_finished(void *data, struct zwlr_data_control_device_v1 *device)
{
    (void)data;
    (void)device;
}

static const struct zwlr_data_control_device_v1_listener control_listener = {
    .data_offer = control_offer,
    .selection = control_selection,
    .primary_selection = control_selection,
    .finished = control_finished,
};

static void offer_mime(void *data, struct wl_data_offer *offer, const char *mime)
{
    (void)offer;
    if (strcmp(mime, "text/plain") == 0)
        ((struct selection_offer *)data)->has_text = 1;
}

static const struct wl_data_offer_listener offer_listener = {
    .offer = offer_mime,
};

static void device_offer(void *data, struct wl_data_device *device, struct wl_data_offer *offer)
{
    (void)device;
    struct target_state *state = data;
    struct selection_offer *entry = calloc(1, sizeof(*entry));
    if (!entry)
        abort();
    entry->proxy = offer;
    entry->next = state->offers;
    state->offers = entry;
    wl_data_offer_add_listener(offer, &offer_listener, entry);
}

static void device_selection(void *data, struct wl_data_device *device, struct wl_data_offer *offer)
{
    (void)device;
    // Retain old proxies to verify that focus loss and source replacement
    // make them inert. A receive queued before dispatching this event is valid.
    ((struct target_state *)data)->selection = offer;
}

static const struct wl_data_device_listener device_listener = {
    .data_offer = device_offer,
    .selection = device_selection,
};

static int focus(unsigned index, int clear_first)
{
    struct data_device_focus_request request = { .index = index, .clear_first = clear_first };
    return invoke_on_server_thread(data_device_focus, &request) && request.focused;
}

static int transfer(const char *name, struct client_connection *target,
                    struct client_connection *source, struct wl_data_offer *offer,
                    struct source_state *state, const char *expected)
{
    int fds[2];
    if (!offer || pipe(fds) != 0)
        return 0;
    const unsigned sends = state->sends;
    wl_data_offer_receive(offer, "text/plain", fds[1]);
    close(fds[1]);
    if (wl_display_roundtrip(target->display) < 0
        || wl_display_roundtrip(source->display) < 0) {
        close(fds[0]);
        return 0;
    }
    char buffer[128];
    const ssize_t size = read(fds[0], buffer, sizeof(buffer));
    close(fds[0]);
    const size_t expected_size = strlen(expected);
    if (size != (ssize_t)expected_size || memcmp(buffer, expected, expected_size) != 0
        || state->sends != sends + (expected_size != 0) || state->write_failed) {
        TEST_ERROR("%s: expected %zu bytes, received %zd, source sends %u\n",
                   name, expected_size, size, state->sends - sends);
        return 0;
    }
    fprintf(stderr, "PASS: %s\n", name);
    return 1;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection source = {0}, target = {0};
    struct xdg_toplevel_client main_window = {0}, peer_window = {0}, other_window = {0};
    struct client_seat source_seat = {0}, target_seat = {0};
    struct target_state target_state = {0};
    struct source_state first = { .payload = "clipboard-focus-中文" };
    struct source_state second = { .payload = "replacement" };
    int result = 1;

    if (!client_connect(&source, socket_name) || !client_connect(&target, socket_name)
        || !client_bind_seat(&source, 1, &source_seat)
        || !client_bind_seat(&target, 1, &target_seat))
        goto cleanup;
    struct zwlr_data_control_manager_v1 *control = client_bind(
        &source, "zwlr_data_control_manager_v1", &zwlr_data_control_manager_v1_interface, 2);
    struct wl_data_device_manager *manager = client_bind(
        &target, "wl_data_device_manager", &wl_data_device_manager_interface, 3);
    if (!control || !manager)
        goto cleanup;
    struct zwlr_data_control_device_v1 *source_device =
        zwlr_data_control_manager_v1_get_data_device(control, source_seat.seat);
    zwlr_data_control_device_v1_add_listener(source_device, &control_listener, NULL);
    struct wl_data_device *target_device = wl_data_device_manager_get_data_device(manager, target_seat.seat);
    wl_data_device_add_listener(target_device, &device_listener, &target_state);

    if (!xdg_toplevel_client_create(&target, &main_window)
        || !xdg_toplevel_client_create(&target, &peer_window)
        || !xdg_toplevel_client_create(&source, &other_window))
        goto cleanup;
    struct zwlr_data_control_source_v1 *first_source = zwlr_data_control_manager_v1_create_data_source(control);
    zwlr_data_control_source_v1_add_listener(first_source, &source_listener, &first);
    zwlr_data_control_source_v1_offer(first_source, "text/plain");
    zwlr_data_control_device_v1_set_selection(source_device, first_source);
    if (wl_display_roundtrip(source.display) < 0 || !focus(0, 0)
        || wl_display_roundtrip(target.display) < 0
        || !target_state.offers || !target_state.offers->has_text
        || !transfer("initial selection", &target, &source, target_state.selection, &first, first.payload))
        goto cleanup;

    struct wl_data_offer *old_offer = target_state.selection;
    if (!focus(1, 0)
        || !transfer("same-client focus before dispatch", &target, &source, old_offer, &first, first.payload))
        goto cleanup;
    old_offer = target_state.selection;
    if (!focus(0, 1)
        || !transfer("same-client focus after temporary clear", &target, &source, old_offer, &first, first.payload))
        goto cleanup;
    old_offer = target_state.selection;
    if (!focus(2, 0)
        || !transfer("other-client focus invalidates old offer", &target, &source, old_offer, &first, ""))
        goto cleanup;
    if (!focus(0, 0) || wl_display_roundtrip(target.display) < 0
        || !transfer("return to client gets valid offer", &target, &source, target_state.selection, &first, first.payload))
        goto cleanup;

    old_offer = target_state.selection;
    struct zwlr_data_control_source_v1 *second_source = zwlr_data_control_manager_v1_create_data_source(control);
    zwlr_data_control_source_v1_add_listener(second_source, &source_listener, &second);
    zwlr_data_control_source_v1_offer(second_source, "text/plain");
    zwlr_data_control_device_v1_set_selection(source_device, second_source);
    if (wl_display_roundtrip(source.display) < 0 || first.cancelled != 1
        || !transfer("replacement invalidates old offer", &target, &source, old_offer, &second, "")
        || !transfer("replacement payload", &target, &source, target_state.selection, &second, second.payload))
        goto cleanup;
    old_offer = target_state.selection;
    zwlr_data_control_source_v1_destroy(second_source);
    if (wl_display_roundtrip(source.display) < 0
        || !transfer("source destruction invalidates old offer", &target, &source, old_offer, &second, "")
        || target_state.selection != NULL)
        goto cleanup;

    zwlr_data_control_source_v1_destroy(first_source);
    zwlr_data_control_device_v1_destroy(source_device);
    zwlr_data_control_manager_v1_destroy(control);
    wl_data_device_release(target_device);
    wl_data_device_manager_destroy(manager);
    result = 0;
cleanup:
    while (target_state.offers) {
        struct selection_offer *entry = target_state.offers;
        target_state.offers = entry->next;
        wl_data_offer_destroy(entry->proxy);
        free(entry);
    }
    xdg_toplevel_client_destroy(&other_window);
    xdg_toplevel_client_destroy(&peer_window);
    xdg_toplevel_client_destroy(&main_window);
    client_seat_destroy(&target_seat);
    client_seat_destroy(&source_seat);
    client_disconnect(&target);
    client_disconnect(&source);
    return result;
}
