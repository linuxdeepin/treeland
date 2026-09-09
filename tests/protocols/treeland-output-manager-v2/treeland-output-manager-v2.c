// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-output-manager-v2.h"
#include "treeland-output-manager-unstable-v2-client-protocol.h"

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
                                   struct wl_output *output)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->primary_output_received = 1;
    ++ctx->primary_output_count;
    ctx->primary_output_obj = output;
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

/* ---- picture control listener ---- */

static void picture_control_result(void *data,
                                   struct treeland_output_picture_control_v2 *control,
                                   uint32_t result)
{
    (void)control;
    struct test_ctx *ctx = data;
    ctx->result_received = 1;
    ctx->result_value = result;
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
    if (!ctx->manager)
        return 0;
    treeland_output_manager_v2_add_listener(ctx->manager, &manager_listener, ctx);
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

/* v2: primary_output event carries a wl_output object (non-null when an
 * output is available) and is emitted once immediately after bind. */
static int primary_output_event_received(struct test_ctx *ctx)
{
    wl_display_roundtrip(ctx->display);
    return ctx->primary_output_received && ctx->primary_output_count == 1
        && ctx->primary_output_obj != NULL;
}

/* v2: set_primary_output takes a wl_output object; a valid enabled output
 * is accepted without protocol error.  When the output is already the
 * primary, the server may not emit primary_output again, so we only check
 * that no primary_output_failed event and no protocol error occur. */
static int set_primary_output_valid(struct test_ctx *ctx)
{
    if (!ctx->manager || !ctx->output)
        return 0;
    ctx->primary_output_failed_received = 0;
    treeland_output_manager_v2_set_primary_output(ctx->manager, ctx->output);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    const struct wl_interface *error_interface = NULL;
    uint32_t error_object_id = 0;
    /* Success: no primary_output_failed event and no protocol error. */
    return !ctx->primary_output_failed_received
        && wl_display_get_protocol_error(ctx->display, &error_interface, &error_object_id) == 0;
}

/* v2: get_picture_control always succeeds for a live wl_output; no protocol
 * error is raised. */
static int get_picture_control_valid_output(struct test_ctx *ctx)
{
    if (!ctx->manager || !ctx->output)
        return 0;
    ctx->picture_control =
        treeland_output_manager_v2_get_picture_control(ctx->manager, ctx->output);
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
    { "manager.bind", manager_bound },
    { "output.bind", output_bound },
    { "manager.event.primary_output", primary_output_event_received },
    { "manager.set_primary_output.valid", set_primary_output_valid },
    { "manager.get_picture_control.valid_output", get_picture_control_valid_output },
    { "picture_control.commit.no_changes_success", commit_no_changes_success },
    { "picture_control.set_color_temperature.out_of_range_error",
      set_color_temperature_out_of_range_error, 1 },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->picture_control) treeland_output_picture_control_v2_destroy(ctx->picture_control);
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
