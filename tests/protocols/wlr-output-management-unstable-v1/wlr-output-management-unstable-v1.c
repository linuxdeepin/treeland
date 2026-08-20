// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "server-bridge-api.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"
#include "wlr-output-management-unstable-v1.h"

#include <stdio.h>
#include <string.h>

extern void output_management_read_server_state(void *data);
extern void output_management_render(void *data);

struct output_events {
    struct zwlr_output_head_v1 *head;
    struct zwlr_output_mode_v1 *mode;
    uint32_t serial;
    int manager_done_count;
    int manager_finished_count;
    int head_name_count;
    int head_description_count;
    int head_enabled_count;
    int head_current_mode_count;
    int head_position_count;
    int head_transform_count;
    int head_scale_count;
    int head_adaptive_sync_count;
    int mode_size_count;
    int mode_refresh_count;
    int mode_preferred_count;
    int configuration_succeeded_count;
    int configuration_failed_count;
    int configuration_cancelled_count;
    int position_x;
    int position_y;
};

static const struct zwlr_output_mode_v1_listener mode_listener;

static void head_name(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    (void)head;
    (void)name;
    ((struct output_events *)data)->head_name_count++;
}

static void head_description(void *data, struct zwlr_output_head_v1 *head, const char *description)
{
    (void)head;
    (void)description;
    ((struct output_events *)data)->head_description_count++;
}

static void head_physical_size(void *data, struct zwlr_output_head_v1 *head, int32_t width, int32_t height)
{
    (void)data;
    (void)head;
    (void)width;
    (void)height;
}

static void head_mode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)head;
    struct output_events *events = data;
    events->mode = mode;
    zwlr_output_mode_v1_add_listener(mode, &mode_listener, events);
}

static void head_enabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    (void)head;
    (void)enabled;
    ((struct output_events *)data)->head_enabled_count++;
}

static void head_current_mode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)head;
    (void)mode;
    ((struct output_events *)data)->head_current_mode_count++;
}

static void head_position(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y)
{
    (void)head;
    struct output_events *events = data;
    events->position_x = x;
    events->position_y = y;
    events->head_position_count++;
}

static void head_transform(void *data, struct zwlr_output_head_v1 *head, int32_t transform)
{
    (void)head;
    (void)transform;
    ((struct output_events *)data)->head_transform_count++;
}

static void head_scale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
    (void)head;
    (void)scale;
    ((struct output_events *)data)->head_scale_count++;
}

static void head_finished(void *data, struct zwlr_output_head_v1 *head)
{
    (void)data;
    (void)head;
}

static void head_make(void *data, struct zwlr_output_head_v1 *head, const char *make)
{
    (void)data;
    (void)head;
    (void)make;
}

static void head_model(void *data, struct zwlr_output_head_v1 *head, const char *model)
{
    (void)data;
    (void)head;
    (void)model;
}

static void head_serial_number(void *data, struct zwlr_output_head_v1 *head, const char *serial_number)
{
    (void)data;
    (void)head;
    (void)serial_number;
}

static void head_adaptive_sync(void *data, struct zwlr_output_head_v1 *head, uint32_t state)
{
    (void)head;
    (void)state;
    ((struct output_events *)data)->head_adaptive_sync_count++;
}

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = head_name,
    .description = head_description,
    .physical_size = head_physical_size,
    .mode = head_mode,
    .enabled = head_enabled,
    .current_mode = head_current_mode,
    .position = head_position,
    .transform = head_transform,
    .scale = head_scale,
    .finished = head_finished,
    .make = head_make,
    .model = head_model,
    .serial_number = head_serial_number,
    .adaptive_sync = head_adaptive_sync,
};

static void mode_size(void *data, struct zwlr_output_mode_v1 *mode, int32_t width, int32_t height)
{
    (void)mode;
    (void)width;
    (void)height;
    ((struct output_events *)data)->mode_size_count++;
}

static void mode_refresh(void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
    (void)mode;
    (void)refresh;
    ((struct output_events *)data)->mode_refresh_count++;
}

static void mode_preferred(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)mode;
    ((struct output_events *)data)->mode_preferred_count++;
}

static void mode_finished(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)data;
    (void)mode;
}

static const struct zwlr_output_mode_v1_listener mode_listener = {
    .size = mode_size,
    .refresh = mode_refresh,
    .preferred = mode_preferred,
    .finished = mode_finished,
};

static void manager_head(void *data, struct zwlr_output_manager_v1 *manager,
                         struct zwlr_output_head_v1 *head)
{
    (void)manager;
    struct output_events *events = data;
    events->head = head;
    zwlr_output_head_v1_add_listener(head, &head_listener, events);
}

static void manager_done(void *data, struct zwlr_output_manager_v1 *manager, uint32_t serial)
{
    (void)manager;
    struct output_events *events = data;
    events->serial = serial;
    events->manager_done_count++;
}

static void manager_finished(void *data, struct zwlr_output_manager_v1 *manager)
{
    (void)manager;
    ((struct output_events *)data)->manager_finished_count++;
}

static const struct zwlr_output_manager_v1_listener manager_listener = {
    .head = manager_head,
    .done = manager_done,
    .finished = manager_finished,
};

static void configuration_succeeded(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)configuration;
    ((struct output_events *)data)->configuration_succeeded_count++;
}

static void configuration_failed(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)configuration;
    ((struct output_events *)data)->configuration_failed_count++;
}

static void configuration_cancelled(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)configuration;
    ((struct output_events *)data)->configuration_cancelled_count++;
}

static const struct zwlr_output_configuration_v1_listener configuration_listener = {
    .succeeded = configuration_succeeded,
    .failed = configuration_failed,
    .cancelled = configuration_cancelled,
};

static int read_server_state(struct output_management_server_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(output_management_read_server_state, state);
}

static struct zwlr_output_configuration_v1 *create_enabled_configuration(
    struct zwlr_output_manager_v1 *manager, struct output_events *events, int x, int y,
    int32_t transform, wl_fixed_t scale)
{
    struct zwlr_output_configuration_v1 *configuration =
        zwlr_output_manager_v1_create_configuration(manager, events->serial);
    if (!configuration)
        return NULL;

    zwlr_output_configuration_v1_add_listener(configuration, &configuration_listener, events);
    struct zwlr_output_configuration_head_v1 *head =
        zwlr_output_configuration_v1_enable_head(configuration, events->head);
    if (!head) {
        zwlr_output_configuration_v1_destroy(configuration);
        return NULL;
    }

    /* The headless backend has no advertised fixed mode. Use its real 1920x1080 custom mode. */
    zwlr_output_configuration_head_v1_set_custom_mode(head, 1920, 1080, 0);
    zwlr_output_configuration_head_v1_set_position(head, x, y);
    zwlr_output_configuration_head_v1_set_transform(head, transform);
    zwlr_output_configuration_head_v1_set_scale(head, scale);
    zwlr_output_configuration_head_v1_set_adaptive_sync(
        head, ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
    return configuration;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct zwlr_output_manager_v1 *manager = NULL;
    struct output_events events = { 0 };
    struct output_management_server_state state = { 0 };
    int stage = 0;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        return 1;

    manager = client_bind(&connection, "zwlr_output_manager_v1",
                                 &zwlr_output_manager_v1_interface, 4);
    if (!manager)
        goto done;
    stage = 1;
    zwlr_output_manager_v1_add_listener(manager, &manager_listener, &events);
    if (wl_display_roundtrip(connection.display) < 0 || !events.head || !events.manager_done_count
        || !events.head_name_count || !events.head_description_count || !events.head_enabled_count
        || !events.head_current_mode_count || !events.head_position_count || !events.head_transform_count
        || !events.head_scale_count || !events.head_adaptive_sync_count || !events.mode
        || !events.mode_size_count)
        goto done;

    stage = 2;
    struct zwlr_output_configuration_v1 *configuration =
        create_enabled_configuration(manager, &events, 0, 0, WL_OUTPUT_TRANSFORM_NORMAL,
                                     wl_fixed_from_int(1));
    if (!configuration)
        goto done;
    stage = 3;
    zwlr_output_configuration_v1_test(configuration);
    if (wl_display_roundtrip(connection.display) < 0 || events.configuration_succeeded_count != 1
        || events.configuration_failed_count || events.configuration_cancelled_count) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;

    stage = 4;
    configuration = zwlr_output_manager_v1_create_configuration(manager, events.serial);
    if (!configuration)
        goto done;
    zwlr_output_configuration_v1_add_listener(configuration, &configuration_listener, &events);
    zwlr_output_configuration_v1_disable_head(configuration, events.head);
    stage = 5;
    zwlr_output_configuration_v1_test(configuration);
    if (wl_display_roundtrip(connection.display) < 0 || events.configuration_succeeded_count != 2
        || events.configuration_failed_count || events.configuration_cancelled_count) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;

    stage = 6;
    configuration = create_enabled_configuration(manager, &events, 37, 53, WL_OUTPUT_TRANSFORM_90,
                                                 wl_fixed_from_int(2));
    if (!configuration)
        goto done;
    stage = 7;
    zwlr_output_configuration_v1_apply(configuration);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(output_management_render, NULL)
        || wl_display_roundtrip(connection.display) < 0 || events.configuration_succeeded_count != 3
        || events.configuration_failed_count || events.configuration_cancelled_count) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;
    stage = 8;
    if (!read_server_state(&state) || state.output_count != 1 || !state.enabled
        || state.x != 37 || state.y != 53 || state.transform != WL_OUTPUT_TRANSFORM_90
        || state.scale_milli != 2000)
        goto done;

    stage = 9;
    configuration = create_enabled_configuration(manager, &events, 0, 0, WL_OUTPUT_TRANSFORM_NORMAL,
                                                 wl_fixed_from_int(1));
    if (!configuration)
        goto done;
    stage = 10;
    zwlr_output_configuration_v1_apply(configuration);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(output_management_render, NULL)
        || wl_display_roundtrip(connection.display) < 0 || events.configuration_succeeded_count != 4
        || events.configuration_failed_count || events.configuration_cancelled_count) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;
    stage = 11;
    if (!read_server_state(&state) || state.x || state.y
        || state.transform != WL_OUTPUT_TRANSFORM_NORMAL || state.scale_milli != 1000)
        goto done;

    stage = 12;
    zwlr_output_manager_v1_stop(manager);
    if (wl_display_roundtrip(connection.display) < 0 || events.manager_finished_count != 1)
        goto done;

    if (events.mode)
        zwlr_output_mode_v1_release(events.mode);
    zwlr_output_head_v1_release(events.head);
    result = 0;

done:
    if (result) {
        read_server_state(&state);
        fprintf(stderr,
                "wlr-output-management failure at stage %d: serial=%u events=(head=%p mode=%p done=%d finished=%d name=%d description=%d enabled=%d current-mode=%d position=%d transform=%d scale=%d adaptive-sync=%d mode-size=%d mode-refresh=%d succeeded=%d failed=%d cancelled=%d) server=(outputs=%d enabled=%d position=%d,%d transform=%d scale-milli=%d pending=%d/%d/%d request=%d/%d/%d/%d)\n",
                stage, events.serial, (void *)events.head, (void *)events.mode,
                events.manager_done_count, events.manager_finished_count, events.head_name_count,
                events.head_description_count, events.head_enabled_count,
                events.head_current_mode_count, events.head_position_count,
                events.head_transform_count, events.head_scale_count,
                events.head_adaptive_sync_count, events.mode_size_count,
                events.mode_refresh_count, events.configuration_succeeded_count,
                events.configuration_failed_count, events.configuration_cancelled_count,
                state.output_count, state.enabled, state.x, state.y,
                state.transform, state.scale_milli, state.pending_state,
                state.pending_transform, state.pending_scale_milli,
                state.request_count, state.last_request_test,
                state.last_request_transform, state.last_request_scale_milli);
    }
    if (manager)
        wl_proxy_destroy((struct wl_proxy *)manager);
    client_disconnect(&connection);
    return result;
}
