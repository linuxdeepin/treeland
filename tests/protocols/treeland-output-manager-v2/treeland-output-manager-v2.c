// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-output-manager-v2.h"
#include "treeland-output-manager-unstable-v2-client-protocol.h"
#include "treeland-output-uuid-unstable-v1-client-protocol.h"
#include "server-bridge-api.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
    /* Non-zero when the case raises a fatal protocol error and the
     * connection is expected to be terminated afterwards. */
    int expect_disconnect;
};

void test_init(struct test_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->result_cap = 32;
    ctx->results = calloc(ctx->result_cap, sizeof(*ctx->results));
}

void test_destroy(struct test_ctx *ctx)
{
    free(ctx->results);
    memset(ctx, 0, sizeof(*ctx));
}

int test_add(struct test_ctx *ctx, const char *name)
{
    if (ctx->result_count == ctx->result_cap) {
        ctx->result_cap *= 2;
        ctx->results = realloc(ctx->results, (size_t)ctx->result_cap * sizeof(*ctx->results));
    }
    const int index = ctx->result_count++;
    ctx->results[index] = (struct test_result) { .name = name };
    return index;
}

void test_fail(struct test_ctx *ctx, int index, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(ctx->results[index].message, TEST_MSG_MAX, format, arguments);
    va_end(arguments);
    ctx->results[index].failed = 1;
}

void test_pass(struct test_ctx *ctx, int index)
{
    ctx->results[index].failed = 0;
}

int test_print_results(struct test_ctx *ctx)
{
    int failed = 0;
    printf("\n=== results ===\n");
    for (int i = 0; i < ctx->result_count; ++i) {
        printf("  [%s] %s", ctx->results[i].failed ? "FAIL" : "PASS", ctx->results[i].name);
        if (ctx->results[i].failed) {
            printf(" -- %s", ctx->results[i].message);
            ++failed;
        }
        printf("\n");
    }
    printf("%d/%d passed\n", ctx->result_count - failed, ctx->result_count);
    return failed == 0;
}

/* ---- manager listener ---- */

static void manager_primary_output(void *data, struct treeland_output_manager_v2 *manager,
                                   const char *uuid)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->primary_output_received = 1;
    ++ctx->primary_output_count;
    snprintf(ctx->primary_output_uuid, sizeof(ctx->primary_output_uuid), "%s", uuid);
}

static void manager_primary_output_failed(void *data,
                                          struct treeland_output_manager_v2 *manager,
                                          uint32_t error)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->primary_output_failed_received = 1;
    ctx->primary_output_failed_error = error;
}

static const struct treeland_output_manager_v2_listener manager_listener = {
    .primary_output = manager_primary_output,
    .primary_output_failed = manager_primary_output_failed,
};

/* ---- output uuid listener ---- */

static void output_uuid_event(void *data, struct treeland_output_uuid_v1 *object,
                              const char *uuid)
{
    (void)object;
    struct test_ctx *ctx = data;
    ctx->uuid_received = 1;
    snprintf(ctx->uuid, sizeof(ctx->uuid), "%s", uuid);
}

static const struct treeland_output_uuid_v1_listener output_uuid_listener = {
    .uuid = output_uuid_event,
};

/* ---- picture control listener ---- */

static void picture_control_result(void *data,
                                   struct treeland_output_picture_control_v2 *control,
                                   uint32_t result)
{
    (void)control;
    struct test_ctx *ctx = data;
    ctx->result_received = 1;
    ctx->result_value = result;
    ++ctx->result_event_count;
}

static void picture_control_color_temperature(void *data,
                                              struct treeland_output_picture_control_v2 *control,
                                              uint32_t temperature)
{
    (void)control;
    struct test_ctx *ctx = data;
    ctx->color_temperature_received = 1;
    ctx->color_temperature_value = temperature;
}

static void picture_control_brightness(void *data,
                                       struct treeland_output_picture_control_v2 *control,
                                       wl_fixed_t brightness)
{
    (void)control;
    struct test_ctx *ctx = data;
    ctx->brightness_received = 1;
    ctx->brightness_value = brightness;
}

static const struct treeland_output_picture_control_v2_listener picture_control_listener = {
    .result = picture_control_result,
    .color_temperature = picture_control_color_temperature,
    .brightness = picture_control_brightness,
};

/* ---- test helpers ---- */

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    ctx->socket_name = socket_name;
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->output = client_bind(&ctx->connection, "wl_output", &wl_output_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_output_manager_v2",
                                      &treeland_output_manager_v2_interface, 1);
    ctx->uuid_manager = client_bind(&ctx->connection, "treeland_output_uuid_manager_v1",
                                           &treeland_output_uuid_manager_v1_interface, 1);
    if (!ctx->manager || !ctx->uuid_manager || !ctx->output)
        return 0;
    treeland_output_manager_v2_add_listener(ctx->manager, &manager_listener, ctx);

    /* v2 addresses outputs by uuid, so fetch it from the uuid protocol. */
    ctx->uuid_object = treeland_output_uuid_manager_v1_get_uuid(ctx->uuid_manager,
                                                                ctx->output);
    if (!ctx->uuid_object)
        return 0;
    treeland_output_uuid_v1_add_listener(ctx->uuid_object, &output_uuid_listener, ctx);
    return 1;
}

static int manager_bound(struct test_ctx *ctx)
{
    return ctx->manager != NULL;
}

static int output_bound(struct test_ctx *ctx)
{
    return ctx->output != NULL;
}

/* v2: primary_output event carries the uuid of the primary output and is
 * emitted once immediately after bind. */
static int primary_output_event_received(struct test_ctx *ctx)
{
    wl_display_roundtrip(ctx->display);
    return ctx->uuid_received && ctx->uuid[0] != '\0'
        && ctx->primary_output_received && ctx->primary_output_count == 1
        && strcmp(ctx->primary_output_uuid, ctx->uuid) == 0;
}

/* v2: set_primary_output takes a uuid string; a valid enabled output is
 * accepted and confirmed by a primary_output event, even when the output is
 * already the primary (the request must always be confirmed). */
static int set_primary_output_valid(struct test_ctx *ctx)
{
    if (!ctx->manager || ctx->uuid[0] == '\0')
        return 0;
    ctx->primary_output_failed_received = 0;
    ctx->primary_output_received = 0;
    ctx->primary_output_count = 0;
    treeland_output_manager_v2_set_primary_output(ctx->manager, ctx->uuid);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    const struct wl_interface *error_interface = NULL;
    uint32_t error_object_id = 0;
    return !ctx->primary_output_failed_received
        && ctx->primary_output_received && ctx->primary_output_count >= 1
        && strcmp(ctx->primary_output_uuid, ctx->uuid) == 0
        && wl_display_get_protocol_error(ctx->display, &error_interface, &error_object_id) == 0;
}

/* v2: an empty or unknown uuid is rejected non-fatally with
 * primary_output_failed(invalid_output == 0); the connection stays alive. */
static int set_primary_output_unknown_failed(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    ctx->primary_output_failed_received = 0;
    ctx->primary_output_failed_error = 0xffffffff;
    treeland_output_manager_v2_set_primary_output(ctx->manager, "00000000-unknown");
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->primary_output_failed_received
        && ctx->primary_output_failed_error == 0; /* invalid_output == 0 */
}

static void disable_output(void *data)
{
    (void)data;
    protocol_test_disable_output();
}

/* v2: a disabled output is rejected non-fatally with
 * primary_output_failed(invalid_output == 0); the connection stays alive. */
static int set_primary_output_disabled_failed(struct test_ctx *ctx)
{
    if (!ctx->manager || ctx->uuid[0] == '\0')
        return 0;
    if (!invoke_on_server_thread(disable_output, NULL))
        return 0;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;

    ctx->primary_output_failed_received = 0;
    ctx->primary_output_failed_error = 0xffffffff;
    treeland_output_manager_v2_set_primary_output(ctx->manager, ctx->uuid);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->primary_output_failed_received
        && ctx->primary_output_failed_error == 0; /* invalid_output == 0 */
}

/* v2: once the output behind the uuid is disabled, commits report
 * commit_result.invalid_output (3) and leave the connection alive. */
static int commit_disabled_output_invalid(struct test_ctx *ctx)
{
    if (!ctx->picture_control)
        return 0;
    ctx->result_received = 0;
    ctx->result_value = 0xffffffff;
    treeland_output_picture_control_v2_commit(ctx->picture_control);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->result_received && ctx->result_value == 3; /* invalid_output */
}

/* v2: the invalid_output delivered by a commit is terminal too: a second
 * commit on the same object produces no event and does not disconnect. */
static int commit_disabled_output_inert_no_more_events(struct test_ctx *ctx)
{
    if (!ctx->picture_control)
        return 0;
    const int before = ctx->result_event_count;
    ctx->result_received = 0;
    ctx->result_value = 0xffffffff;
    treeland_output_picture_control_v2_commit(ctx->picture_control);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return !ctx->result_received && ctx->result_event_count == before;
}

/* v2: get_picture_control always succeeds for a known uuid; no protocol
 * error is raised. */
static int get_picture_control_valid_uuid(struct test_ctx *ctx)
{
    if (!ctx->manager || ctx->uuid[0] == '\0')
        return 0;
    ctx->picture_control =
        treeland_output_manager_v2_get_picture_control(ctx->manager, ctx->uuid);
    if (!ctx->picture_control)
        return 0;
    treeland_output_picture_control_v2_add_listener(ctx->picture_control,
                                                     &picture_control_listener, ctx);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    const struct wl_interface *error_interface = NULL;
    uint32_t error_object_id = 0;
    return wl_display_get_protocol_error(ctx->display, &error_interface, &error_object_id) == 0;
}

/* v2: an object created for a live enabled output reports its values with the
 * initial color_temperature / brightness events, never with a result event at
 * creation time. */
static int get_picture_control_valid_uuid_no_result_at_creation(struct test_ctx *ctx)
{
    if (!ctx->picture_control)
        return 0;
    ctx->result_received = 0;
    ctx->result_event_count = 0;
    ctx->result_value = 0xffffffff;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return !ctx->result_received && ctx->result_event_count == 0;
}

/* v2: the initial brightness event must stay inside the protocol range
 * [0.0, 100.0]; otherwise a client echoing it back is disconnected by the
 * fatal invalid_brightness error. */
static int picture_control_initial_brightness_in_range(struct test_ctx *ctx)
{
    if (!ctx->picture_control)
        return 0;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->brightness_received
        && ctx->brightness_value >= 0
        && ctx->brightness_value <= wl_fixed_from_double(100.0);
}

/* v2: an unknown uuid still creates the object, but invalid_output is reported
 * immediately, without waiting for a commit, and no initial color_temperature
 * / brightness events are sent. */
static int get_picture_control_unknown_uuid_invalid_output(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    ctx->unknown_picture_control =
        treeland_output_manager_v2_get_picture_control(ctx->manager, "00000000-unknown");
    if (!ctx->unknown_picture_control)
        return 0;
    treeland_output_picture_control_v2_add_listener(ctx->unknown_picture_control,
                                                     &picture_control_listener, ctx);
    ctx->result_received = 0;
    ctx->result_event_count = 0;
    ctx->result_value = 0xffffffff;
    ctx->brightness_received = 0;
    ctx->color_temperature_received = 0;
    /* Deliberately no commit: the result must arrive on this roundtrip. */
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->result_received && ctx->result_event_count == 1
        && ctx->result_value == 3 /* invalid_output */
        && !ctx->brightness_received && !ctx->color_temperature_received;
}

/* v2: invalid_output is terminal. On the inert object only destroy stays
 * valid: commit and set_* produce no further events and keep the connection
 * alive. */
static int invalid_output_inert_ignores_commit(struct test_ctx *ctx)
{
    if (!ctx->unknown_picture_control)
        return 0;
    const int before = ctx->result_event_count;
    ctx->result_received = 0;
    ctx->result_value = 0xffffffff;
    treeland_output_picture_control_v2_commit(ctx->unknown_picture_control);
    treeland_output_picture_control_v2_set_brightness(ctx->unknown_picture_control,
                                                      wl_fixed_from_double(50.0));
    treeland_output_picture_control_v2_set_color_temperature(ctx->unknown_picture_control, 4000);
    treeland_output_picture_control_v2_commit(ctx->unknown_picture_control);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0; /* an inert object must not terminate the connection */
    return !ctx->result_received && ctx->result_event_count == before
        && !ctx->brightness_received && !ctx->color_temperature_received;
}

/* v2: commit with no pending changes succeeds (commit_result.success == 0). */
static int commit_no_changes_success(struct test_ctx *ctx)
{
    if (!ctx->picture_control)
        return 0;
    ctx->result_received = 0;
    ctx->result_value = 0xffffffff;
    treeland_output_picture_control_v2_commit(ctx->picture_control);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->result_received && ctx->result_value == 0; /* success == 0 */
}

/* v2: an out-of-range color temperature raises the fatal
 * invalid_color_temperature protocol error (value 0) on the
 * set_color_temperature request itself; the connection is terminated and no
 * result event is produced. */
static int set_color_temperature_out_of_range_error(struct test_ctx *ctx)
{
    if (!ctx->picture_control)
        return 0;
    ctx->result_received = 0;
    ctx->result_value = 0xffffffff;
    treeland_output_picture_control_v2_set_color_temperature(ctx->picture_control, 500);
    if (wl_display_roundtrip(ctx->display) >= 0)
        return 0; /* the fatal protocol error must terminate the connection */
    const struct wl_interface *error_interface = NULL;
    uint32_t error_object_id = 0;
    const uint32_t code =
        wl_display_get_protocol_error(ctx->display, &error_interface, &error_object_id);
    return !ctx->result_received
        && code == 0 /* error.invalid_color_temperature */
        && error_interface == &treeland_output_picture_control_v2_interface;
}

static const struct test_case cases[] = {
    { "manager.bind", manager_bound, 0 },
    { "output.bind", output_bound, 0 },
    { "manager.event.primary_output", primary_output_event_received, 0 },
    { "manager.set_primary_output.valid", set_primary_output_valid, 0 },
    { "manager.set_primary_output.unknown_failed", set_primary_output_unknown_failed, 0 },
    { "manager.get_picture_control.valid_uuid", get_picture_control_valid_uuid, 0 },
    { "manager.get_picture_control.valid_uuid_no_result_at_creation",
      get_picture_control_valid_uuid_no_result_at_creation, 0 },
    { "picture_control.event.initial_brightness_in_range",
      picture_control_initial_brightness_in_range, 0 },
    { "picture_control.commit.no_changes_success", commit_no_changes_success, 0 },
    { "manager.get_picture_control.unknown_uuid_invalid_output",
      get_picture_control_unknown_uuid_invalid_output, 0 },
    { "picture_control.invalid_output.inert_ignores_commit",
      invalid_output_inert_ignores_commit, 0 },
    { "manager.set_primary_output.disabled_failed", set_primary_output_disabled_failed, 0 },
    { "picture_control.commit.disabled_output_invalid",
      commit_disabled_output_invalid, 0 },
    { "picture_control.commit.disabled_output_inert_no_more_events",
      commit_disabled_output_inert_no_more_events, 0 },
    { "picture_control.set_color_temperature.out_of_range_error",
      set_color_temperature_out_of_range_error, 1 },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->unknown_picture_control)
        treeland_output_picture_control_v2_destroy(ctx->unknown_picture_control);
    if (ctx->picture_control) treeland_output_picture_control_v2_destroy(ctx->picture_control);
    if (ctx->uuid_object) treeland_output_uuid_v1_destroy(ctx->uuid_object);
    if (ctx->uuid_manager) treeland_output_uuid_manager_v1_destroy(ctx->uuid_manager);
    if (ctx->manager) treeland_output_manager_v2_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_output_manager_v2\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");
        if (!cases[i].expect_disconnect
            && wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
