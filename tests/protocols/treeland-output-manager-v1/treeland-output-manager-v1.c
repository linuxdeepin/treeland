/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Pure-C Wayland client for the treeland-output-manager-v1 protocol test.
 */
#include "treeland-output-manager-v1.h"
#include "treeland-output-manager-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
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

static void manager_primary_output(void *data, struct treeland_output_manager_v1 *manager,
                                   const char *output_name)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->primary_output_received = 1;
    ++ctx->primary_output_count;
    if (output_name)
        strncpy(ctx->primary_output_name, output_name, sizeof(ctx->primary_output_name) - 1);
}

static const struct treeland_output_manager_v1_listener manager_listener = {
    .primary_output = manager_primary_output,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    ctx->socket_name = socket_name;
    if (!protocol_test_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->output = protocol_test_bind(&ctx->connection, "wl_output", &wl_output_interface, 1);
    ctx->manager = protocol_test_bind(&ctx->connection, "treeland_output_manager_v1",
                                      &treeland_output_manager_v1_interface, 2);
    if (!ctx->manager)
        return 0;
    treeland_output_manager_v1_add_listener(ctx->manager, &manager_listener, ctx);
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

/* The server sends primary_output once per bind with the current primary
 * output's name. The minimal fixture registers no primary output, so the
 * event must arrive with an empty name. */
static int primary_output_event_received(struct test_ctx *ctx)
{
    wl_display_roundtrip(ctx->display);
    return ctx->primary_output_received && ctx->primary_output_count == 1
        && ctx->primary_output_name[0] == '\0';
}

/* set_primary_output with a name no output matches is silently ignored: the
 * connection must survive the roundtrip and no primary_output event may be
 * re-sent. */
static int set_primary_output_unknown(struct test_ctx *ctx)
{
    const int before = ctx->primary_output_count;
    treeland_output_manager_v1_set_primary_output(ctx->manager, "no-such-output");
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->primary_output_count == before;
}

/* get_color_control with a genuine wl_output: the module resolves treeland's
 * Output wrapper through Helper::getOutput(), which the minimal fixture does
 * not register, so the server replies with an INVALID_OBJECT protocol error
 * on the manager resource. This must be the last case: the protocol error
 * terminates the connection, and the case reconnects so the harness roundtrip
 * and cleanup still work. */
static int get_color_control_missing_output(struct test_ctx *ctx)
{
    if (!ctx->manager || !ctx->output)
        return 0;
    ctx->color_control =
        treeland_output_manager_v1_get_color_control(ctx->manager, ctx->output);
    if (!ctx->color_control)
        return 0;

    /* Delivers the request; the server posts the protocol error. */
    wl_display_roundtrip(ctx->display);

    const struct wl_interface *error_interface = NULL;
    uint32_t error_object_id = 0;
    const uint32_t code =
        wl_display_get_protocol_error(ctx->display, &error_interface, &error_object_id);
    if (code != WL_DISPLAY_ERROR_INVALID_OBJECT)
        return 0;
    if (error_interface != &treeland_output_manager_v1_interface)
        return 0;

    /* The protocol error terminated the connection; reopen a fresh one. */
    ctx->color_control = NULL;
    protocol_test_disconnect(&ctx->connection);
    return connect_client(ctx, ctx->socket_name);
}

static const struct test_case cases[] = {
    { "manager.bind", manager_bound },
    { "output.bind", output_bound },
    { "manager.event.primary_output", primary_output_event_received },
    { "manager.set_primary_output.unknown_name", set_primary_output_unknown },
    { "manager.get_color_control.missing_output_error", get_color_control_missing_output },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->color_control) treeland_output_color_control_v1_destroy(ctx->color_control);
    if (ctx->manager) treeland_output_manager_v1_destroy(ctx->manager);
    protocol_test_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_output_manager_v1\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");
        if (wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
