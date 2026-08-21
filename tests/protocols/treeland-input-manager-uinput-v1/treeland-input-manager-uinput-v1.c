// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-input-manager-uinput-v1.h"
#include "treeland-input-manager-unstable-v1-client-protocol.h"

#include <stdio.h>

extern void input_manager_uinput_read_state(void *data);
extern void input_manager_uinput_destroy(void *data);

struct capability_state {
    uint32_t available_types;
    uint32_t unavailable_types;
    int available_count;
    int unavailable_count;
};

static void capability_available(void *data,
                                 struct treeland_input_manager_v1 *manager,
                                 uint32_t types,
                                 struct wl_seat *seat)
{
    (void)manager;
    (void)seat;
    struct capability_state *state = data;
    state->available_types |= types;
    ++state->available_count;
}

static void capability_unavailable(void *data,
                                   struct treeland_input_manager_v1 *manager,
                                   uint32_t types,
                                   struct wl_seat *seat)
{
    (void)manager;
    (void)seat;
    struct capability_state *state = data;
    state->unavailable_types |= types;
    ++state->unavailable_count;
}

static const struct treeland_input_manager_v1_listener manager_listener = {
    .capability_available = capability_available,
    .capability_unavailable = capability_unavailable,
};

static int read_state(struct input_manager_uinput_state *state)
{
    return invoke_on_server_thread(input_manager_uinput_read_state, state);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct treeland_input_manager_v1 *manager = client_bind(
        &connection, "treeland_input_manager_v1", &treeland_input_manager_v1_interface, 1);
    if (!seat || !manager)
        goto failed;

    struct capability_state capabilities = {0};
    treeland_input_manager_v1_add_listener(manager, &manager_listener, &capabilities);
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;

    struct input_manager_uinput_state state = {0};
    if (!read_state(&state) || !state.created || !state.keyboard_added
        || capabilities.available_types != TREELAND_INPUT_MANAGER_V1_DEVICE_TYPE_KEYBOARD) {
        fprintf(stderr, "input-manager uinput: keyboard did not reach WBackend/input-manager\n");
        goto failed;
    }

    if (!invoke_on_server_thread(input_manager_uinput_destroy, NULL)
        || wl_display_roundtrip(connection.display) < 0)
        goto failed;

    if (!read_state(&state) || !state.keyboard_removed
        || capabilities.unavailable_types != TREELAND_INPUT_MANAGER_V1_DEVICE_TYPE_KEYBOARD) {
        fprintf(stderr, "input-manager uinput: keyboard removal did not reach input-manager\n");
        goto failed;
    }

    treeland_input_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    client_disconnect(&connection);
    return 0;

failed:
    client_disconnect(&connection);
    return 1;
}
