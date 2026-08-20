// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "server-bridge-api.h"
#include "wlr-output-power-management-unstable-v1-client-protocol.h"
#include "wlr-output-power-management-unstable-v1.h"

#include <string.h>
#include <stdio.h>

extern void output_power_read_server_state(void *data);

struct power_events {
    int mode_count;
    int failed_count;
    uint32_t last_mode;
};

static void power_mode(void *data, struct zwlr_output_power_v1 *power, uint32_t mode)
{
    (void)power;
    struct power_events *events = data;
    events->mode_count++;
    events->last_mode = mode;
}

static void power_failed(void *data, struct zwlr_output_power_v1 *power)
{
    (void)power;
    ((struct power_events *)data)->failed_count++;
}

static const struct zwlr_output_power_v1_listener power_listener = {
    .mode = power_mode,
    .failed = power_failed,
};

static int read_server_state(struct output_power_server_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(output_power_read_server_state, state);
}

static int invalid_mode_disconnects(const char *socket_name)
{
    struct client_connection connection;
    struct zwlr_output_power_manager_v1 *manager = NULL;
    struct zwlr_output_power_v1 *power = NULL;
    struct wl_output *output = NULL;
    struct power_events events = { 0 };
    int result = 1;

    if (!client_connect(&connection, socket_name))
        return 1;
    output = client_bind(&connection, "wl_output", &wl_output_interface, 1);
    manager = client_bind(&connection, "zwlr_output_power_manager_v1",
                                 &zwlr_output_power_manager_v1_interface, 1);
    if (!output || !manager)
        goto done;
    power = zwlr_output_power_manager_v1_get_output_power(manager, output);
    if (!power)
        goto done;
    zwlr_output_power_v1_add_listener(power, &power_listener, &events);
    if (wl_display_roundtrip(connection.display) < 0 || events.mode_count != 1
        || events.last_mode != ZWLR_OUTPUT_POWER_V1_MODE_ON)
        goto done;

    zwlr_output_power_v1_set_mode(power, 2);
    result = wl_display_roundtrip(connection.display) < 0 ? 0 : 1;

done:
    client_disconnect(&connection);
    return result;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct zwlr_output_power_manager_v1 *manager = NULL;
    struct zwlr_output_power_v1 *power = NULL;
    struct zwlr_output_power_v1 *second_power = NULL;
    struct wl_output *output = NULL;
    struct power_events events = { 0 };
    struct power_events second_events = { 0 };
    struct output_power_server_state state;
    int result = 1;
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;

    output = client_bind(&connection, "wl_output", &wl_output_interface, 1);
    manager = client_bind(&connection, "zwlr_output_power_manager_v1",
                                 &zwlr_output_power_manager_v1_interface, 1);
    if (!output || !manager)
        goto done;
    stage = 1;

    power = zwlr_output_power_manager_v1_get_output_power(manager, output);
    if (!power)
        goto done;
    zwlr_output_power_v1_add_listener(power, &power_listener, &events);
    if (wl_display_roundtrip(connection.display) < 0 || events.mode_count != 1
        || events.last_mode != ZWLR_OUTPUT_POWER_V1_MODE_ON || events.failed_count)
        goto done;

    second_power = zwlr_output_power_manager_v1_get_output_power(manager, output);
    if (!second_power)
        goto done;
    stage = 2;
    zwlr_output_power_v1_add_listener(second_power, &power_listener, &second_events);
    if (wl_display_roundtrip(connection.display) < 0 || second_events.failed_count != 1
        || second_events.mode_count)
        goto done;
    zwlr_output_power_v1_destroy(second_power);
    second_power = NULL;

    zwlr_output_power_manager_v1_destroy(manager);
    manager = NULL;
    zwlr_output_power_v1_set_mode(power, ZWLR_OUTPUT_POWER_V1_MODE_OFF);
    stage = 3;
    if (wl_display_roundtrip(connection.display) < 0 || events.mode_count != 2
        || events.last_mode != ZWLR_OUTPUT_POWER_V1_MODE_OFF || events.failed_count
        || !read_server_state(&state) || state.output_count != 1 || state.enabled)
        goto done;

    zwlr_output_power_v1_set_mode(power, ZWLR_OUTPUT_POWER_V1_MODE_ON);
    stage = 4;
    if (wl_display_roundtrip(connection.display) < 0 || events.mode_count != 3
        || events.last_mode != ZWLR_OUTPUT_POWER_V1_MODE_ON || events.failed_count
        || !read_server_state(&state) || state.output_count != 1 || !state.enabled)
        goto done;

    zwlr_output_power_v1_destroy(power);
    power = NULL;
    wl_output_destroy(output);
    output = NULL;
    client_disconnect(&connection);
    return invalid_mode_disconnects(socket_name);

done:
    if (result) {
        read_server_state(&state);
        fprintf(stderr,
                "wlr-output-power failure at stage %d: events=(mode=%d last=%u failed=%d second-mode=%d second-failed=%d) server=(outputs=%d enabled=%d)\n",
                stage, events.mode_count, events.last_mode, events.failed_count,
                second_events.mode_count, second_events.failed_count,
                state.output_count, state.enabled);
    }
    if (second_power)
        zwlr_output_power_v1_destroy(second_power);
    if (power)
        zwlr_output_power_v1_destroy(power);
    if (manager)
        zwlr_output_power_manager_v1_destroy(manager);
    if (output)
        wl_output_destroy(output);
    client_disconnect(&connection);
    return result;
}
