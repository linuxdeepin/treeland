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
extern void output_management_set_output_enabled(void *data);

struct output_events;

// The metadata a head must have described itself with before it can be used.
// Tracked per head: an aggregate counter would let one head's events hide a
// missing event of the other.
enum head_metadata {
    HEAD_META_NAME = 1u << 0,
    HEAD_META_DESCRIPTION = 1u << 1,
    HEAD_META_MODE = 1u << 2,
    HEAD_META_MODE_SIZE = 1u << 3,
    HEAD_META_ENABLED = 1u << 4,
    HEAD_META_CURRENT_MODE = 1u << 5,
    HEAD_META_POSITION = 1u << 6,
    HEAD_META_TRANSFORM = 1u << 7,
    HEAD_META_SCALE = 1u << 8,
    HEAD_META_ADAPTIVE_SYNC = 1u << 9,
    HEAD_META_REQUIRED = (1u << 10) - 1,
};

struct head_slot {
    struct output_events *events;
    struct zwlr_output_head_v1 *head;
    struct zwlr_output_mode_v1 *mode;
    unsigned metadata;
    char name[OUTPUT_MANAGEMENT_MAX_NAME];
};

struct output_events {
    struct head_slot heads[OUTPUT_MANAGEMENT_MAX_OUTPUTS];
    int head_count;
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
};

static const struct zwlr_output_mode_v1_listener mode_listener;

static void head_name(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    (void)head;
    struct head_slot *slot = data;
    snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
    slot->metadata |= HEAD_META_NAME;
    slot->events->head_name_count++;
}

static void head_description(void *data, struct zwlr_output_head_v1 *head, const char *description)
{
    (void)head;
    (void)description;
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_DESCRIPTION;
    slot->events->head_description_count++;
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
    struct head_slot *slot = data;
    if (!slot->mode)
        slot->mode = mode;
    slot->metadata |= HEAD_META_MODE;
    zwlr_output_mode_v1_add_listener(mode, &mode_listener, slot);
}

static void head_enabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    (void)head;
    (void)enabled;
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_ENABLED;
    slot->events->head_enabled_count++;
}

static void head_current_mode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)head;
    (void)mode;
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_CURRENT_MODE;
    slot->events->head_current_mode_count++;
}

static void head_position(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y)
{
    (void)head;
    (void)x;
    (void)y;
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_POSITION;
    slot->events->head_position_count++;
}

static void head_transform(void *data, struct zwlr_output_head_v1 *head, int32_t transform)
{
    (void)head;
    (void)transform;
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_TRANSFORM;
    slot->events->head_transform_count++;
}

static void head_scale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
    (void)head;
    (void)scale;
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_SCALE;
    slot->events->head_scale_count++;
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
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_ADAPTIVE_SYNC;
    slot->events->head_adaptive_sync_count++;
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
    struct head_slot *slot = data;
    slot->metadata |= HEAD_META_MODE_SIZE;
    slot->events->mode_size_count++;
}

static void mode_refresh(void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
    (void)mode;
    (void)refresh;
    ((struct head_slot *)data)->events->mode_refresh_count++;
}

static void mode_preferred(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)mode;
    ((struct head_slot *)data)->events->mode_preferred_count++;
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
    if (events->head_count >= OUTPUT_MANAGEMENT_MAX_OUTPUTS)
        return;

    struct head_slot *slot = &events->heads[events->head_count++];
    memset(slot, 0, sizeof(*slot));
    slot->events = events;
    slot->head = head;
    zwlr_output_head_v1_add_listener(head, &head_listener, slot);
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

static const struct output_management_output_state *find_output(
    const struct output_management_server_state *state, const char *name)
{
    for (int i = 0; i < state->output_count; ++i) {
        if (strcmp(state->outputs[i].name, name) == 0)
            return &state->outputs[i];
    }
    return NULL;
}

// Creates a configuration for one head. The headless backend has no advertised
// fixed mode, so the 1920x1080 custom mode it really runs with is used.
static struct zwlr_output_configuration_v1 *create_head_configuration(
    struct zwlr_output_manager_v1 *manager, struct output_events *events, struct head_slot *slot,
    int x, int y, int32_t transform, wl_fixed_t scale, uint32_t adaptive_sync)
{
    struct zwlr_output_configuration_v1 *configuration =
        zwlr_output_manager_v1_create_configuration(manager, events->serial);
    if (!configuration)
        return NULL;
    zwlr_output_configuration_v1_add_listener(configuration, &configuration_listener, events);

    struct zwlr_output_configuration_head_v1 *head =
        zwlr_output_configuration_v1_enable_head(configuration, slot->head);
    if (!head) {
        zwlr_output_configuration_v1_destroy(configuration);
        return NULL;
    }

    zwlr_output_configuration_head_v1_set_custom_mode(head, 1920, 1080, 0);
    zwlr_output_configuration_head_v1_set_position(head, x, y);
    zwlr_output_configuration_head_v1_set_transform(head, transform);
    zwlr_output_configuration_head_v1_set_scale(head, scale);
    zwlr_output_configuration_head_v1_set_adaptive_sync(head, adaptive_sync);
    return configuration;
}

// Completes the apply: the compositor answers once every head of the
// configuration committed, and the commits run on output frames.
static int finish_apply(struct client_connection *connection, struct output_events *events,
                        int expect_succeeded, int expect_failed)
{
    const int succeeded = events->configuration_succeeded_count;
    const int failed = events->configuration_failed_count;

    for (int i = 0; i < 10; ++i) {
        if (events->configuration_succeeded_count != succeeded
            || events->configuration_failed_count != failed)
            break;
        if (!invoke_on_server_thread(output_management_render, NULL))
            return 0;
        if (wl_display_roundtrip(connection->display) < 0)
            return 0;
    }

    const int newSucceeded = events->configuration_succeeded_count - succeeded;
    const int newFailed = events->configuration_failed_count - failed;
    if (newSucceeded != expect_succeeded || newFailed != expect_failed
        || events->configuration_cancelled_count != 0) {
        fprintf(stderr,
                "unexpected apply result: succeeded=%d failed=%d cancelled=%d"
                " (expected succeeded=%d failed=%d)\n",
                newSucceeded, newFailed, events->configuration_cancelled_count,
                expect_succeeded, expect_failed);
        return 0;
    }
    return 1;
}

// Switches both fixture outputs off behind the protocol's back, so the next
// apply has to turn them back on: that is the path in which a head commits
// while the later heads are still being prepared.
static int disable_all_outputs(struct client_connection *connection)
{
    struct output_management_server_state state;
    if (!read_server_state(&state))
        return 0;

    struct output_management_output_request request = { .enabled = 0 };
    for (int i = 0; i < state.output_count; ++i) {
        snprintf(request.name, sizeof(request.name), "%s", state.outputs[i].name);
        if (!invoke_on_server_thread(output_management_set_output_enabled, &request))
            return 0;
    }

    if (!read_server_state(&state))
        return 0;
    for (int i = 0; i < state.output_count; ++i) {
        if (state.outputs[i].enabled)
            return 0;
    }
    // Consume the serial the compositor published for the new state.
    return wl_display_roundtrip(connection->display) >= 0;
}

// One apply that enables two heads, where only one of them can commit.
// BAD_FIRST selects which head is requested first: the compositor commits the
// heads in its own order, so running both orders keeps the case independent of
// it. Either way the apply must report failed, never a premature succeeded
// after the head that does commit.
static int apply_two_heads_one_fails(struct client_connection *connection,
                                     struct zwlr_output_manager_v1 *manager,
                                     struct output_events *events,
                                     struct head_slot *good, struct head_slot *bad,
                                     int bad_first)
{
    struct zwlr_output_configuration_v1 *configuration =
        zwlr_output_manager_v1_create_configuration(manager, events->serial);
    if (!configuration)
        return 0;
    zwlr_output_configuration_v1_add_listener(configuration, &configuration_listener, events);

    struct head_slot *order[2] = { bad_first ? bad : good, bad_first ? good : bad };
    for (int i = 0; i < 2; ++i) {
        const int failing = order[i] == bad;
        struct zwlr_output_configuration_head_v1 *head =
            zwlr_output_configuration_v1_enable_head(configuration, order[i]->head);
        if (!head) {
            zwlr_output_configuration_v1_destroy(configuration);
            return 0;
        }

        // The headless backend refuses a commit carrying adaptive sync, so the
        // failing head's commit really fails.
        zwlr_output_configuration_head_v1_set_custom_mode(head, 1920, 1080, 0);
        zwlr_output_configuration_head_v1_set_position(head, failing ? 1920 : 0, 0);
        zwlr_output_configuration_head_v1_set_transform(head, WL_OUTPUT_TRANSFORM_NORMAL);
        zwlr_output_configuration_head_v1_set_scale(head, wl_fixed_from_int(1));
        zwlr_output_configuration_head_v1_set_adaptive_sync(
            head, failing ? ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED
                          : ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
    }

    zwlr_output_configuration_v1_apply(configuration);
    const int ok = finish_apply(connection, events, 0, 1);
    zwlr_output_configuration_v1_destroy(configuration);
    if (!ok || wl_display_roundtrip(connection->display) < 0)
        return 0;
    return 1;
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
    if (wl_display_roundtrip(connection.display) < 0 || events.head_count != 2
        || !events.manager_done_count)
        goto done;
    // Every head must have described itself completely; checking the union of
    // both heads' events would let one head hide a missing event of the other.
    for (int i = 0; i < events.head_count; ++i) {
        const struct head_slot *slot = &events.heads[i];
        if (!slot->head || !slot->mode || !slot->name[0]
            || (slot->metadata & HEAD_META_REQUIRED) != HEAD_META_REQUIRED)
            goto done;
    }

    struct head_slot *first = &events.heads[0];
    struct head_slot *second = &events.heads[1];

    stage = 2;
    struct zwlr_output_configuration_v1 *configuration =
        create_head_configuration(manager, &events, first, 0, 0, WL_OUTPUT_TRANSFORM_NORMAL,
                                  wl_fixed_from_int(1),
                                  ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
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
    zwlr_output_configuration_v1_disable_head(configuration, first->head);
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
    configuration = create_head_configuration(manager, &events, first, 37, 53,
                                              WL_OUTPUT_TRANSFORM_90, wl_fixed_from_int(2),
                                              ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
    if (!configuration)
        goto done;
    stage = 7;
    zwlr_output_configuration_v1_apply(configuration);
    if (!finish_apply(&connection, &events, 1, 0)) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;
    stage = 8;
    if (!read_server_state(&state) || state.output_count != 2) {
        goto done;
    }
    {
        const struct output_management_output_state *output = find_output(&state, first->name);
        if (!output || !output->enabled || output->x != 37 || output->y != 53
            || output->transform != WL_OUTPUT_TRANSFORM_90 || output->scale_milli != 2000)
            goto done;
    }

    stage = 9;
    configuration = create_head_configuration(manager, &events, first, 0, 0,
                                              WL_OUTPUT_TRANSFORM_NORMAL, wl_fixed_from_int(1),
                                              ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
    if (!configuration)
        goto done;
    stage = 10;
    zwlr_output_configuration_v1_apply(configuration);
    if (!finish_apply(&connection, &events, 1, 0)) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;
    stage = 11;
    if (!read_server_state(&state))
        goto done;
    {
        const struct output_management_output_state *output = find_output(&state, first->name);
        if (!output || output->x || output->y
            || output->transform != WL_OUTPUT_TRANSFORM_NORMAL || output->scale_milli != 1000)
            goto done;
    }

    // One apply may enable several heads. It is applied as a whole: when one
    // head's commit fails, the client must be told "failed", even though the
    // other head committed successfully before that failure. Both head orders
    // run, so the case does not depend on the order the compositor stores and
    // commits the heads in.
    stage = 13;
    if (!disable_all_outputs(&connection)
        || !apply_two_heads_one_fails(&connection, manager, &events, first, second, 0))
        goto done;
    stage = 14;
    if (!disable_all_outputs(&connection)
        || !apply_two_heads_one_fails(&connection, manager, &events, first, second, 1))
        goto done;

    // Both heads in one apply again, this time every head can commit: the
    // second head keeps the configuration the client asked for.
    stage = 15;
    configuration = create_head_configuration(manager, &events, first, 0, 0,
                                              WL_OUTPUT_TRANSFORM_NORMAL, wl_fixed_from_int(1),
                                              ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
    if (!configuration)
        goto done;
    {
        struct zwlr_output_configuration_head_v1 *head =
            zwlr_output_configuration_v1_enable_head(configuration, second->head);
        if (!head) {
            zwlr_output_configuration_v1_destroy(configuration);
            goto done;
        }
        zwlr_output_configuration_head_v1_set_custom_mode(head, 1920, 1080, 0);
        zwlr_output_configuration_head_v1_set_position(head, 1920, 0);
        zwlr_output_configuration_head_v1_set_transform(head, WL_OUTPUT_TRANSFORM_NORMAL);
        zwlr_output_configuration_head_v1_set_scale(head, wl_fixed_from_int(1));
        zwlr_output_configuration_head_v1_set_adaptive_sync(
            head, ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
    }
    stage = 16;
    zwlr_output_configuration_v1_apply(configuration);
    if (!finish_apply(&connection, &events, 1, 0)) {
        zwlr_output_configuration_v1_destroy(configuration);
        goto done;
    }
    zwlr_output_configuration_v1_destroy(configuration);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;
    stage = 17;
    if (!read_server_state(&state))
        goto done;
    {
        const struct output_management_output_state *firstOutput =
            find_output(&state, first->name);
        const struct output_management_output_state *secondOutput =
            find_output(&state, second->name);
        if (!firstOutput || !secondOutput || !firstOutput->enabled || !secondOutput->enabled
            || firstOutput->x != 0 || firstOutput->y != 0
            || secondOutput->x != 1920 || secondOutput->y != 0)
            goto done;
    }

    stage = 18;
    zwlr_output_manager_v1_stop(manager);
    if (wl_display_roundtrip(connection.display) < 0 || events.manager_finished_count != 1)
        goto done;

    for (int i = 0; i < events.head_count; ++i) {
        if (events.heads[i].mode)
            zwlr_output_mode_v1_release(events.heads[i].mode);
        if (events.heads[i].head)
            zwlr_output_head_v1_release(events.heads[i].head);
    }
    result = 0;

done:
    if (result) {
        read_server_state(&state);
        fprintf(stderr,
                "wlr-output-management failure at stage %d: serial=%u events=(heads=%d done=%d finished=%d name=%d description=%d enabled=%d current-mode=%d position=%d transform=%d scale=%d adaptive-sync=%d mode-size=%d mode-refresh=%d succeeded=%d failed=%d cancelled=%d)\n",
                stage, events.serial, events.head_count, events.manager_done_count,
                events.manager_finished_count, events.head_name_count,
                events.head_description_count, events.head_enabled_count,
                events.head_current_mode_count, events.head_position_count,
                events.head_transform_count, events.head_scale_count,
                events.head_adaptive_sync_count, events.mode_size_count,
                events.mode_refresh_count, events.configuration_succeeded_count,
                events.configuration_failed_count, events.configuration_cancelled_count);
        for (int i = 0; i < events.head_count; ++i) {
            fprintf(stderr, "  head %d: name=%s metadata=0x%x/0x%x mode=%p\n",
                    i, events.heads[i].name, events.heads[i].metadata,
                    (unsigned)HEAD_META_REQUIRED, (void *)events.heads[i].mode);
        }
        for (int i = 0; i < state.output_count; ++i) {
            const struct output_management_output_state *output = &state.outputs[i];
            fprintf(stderr,
                    "  server output %d: name=%s enabled=%d position=%d,%d transform=%d scale-milli=%d pending=%d/%d/%d\n",
                    i, output->name, output->enabled, output->x, output->y, output->transform,
                    output->scale_milli, output->pending_state, output->pending_transform,
                    output->pending_scale_milli);
        }
        fprintf(stderr, "  server requests: count=%d last-test=%d transform=%d scale-milli=%d\n",
                state.request_count, state.last_request_test, state.last_request_transform,
                state.last_request_scale_milli);
    }
    if (manager)
        wl_proxy_destroy((struct wl_proxy *)manager);
    client_disconnect(&connection);
    return result;
}
