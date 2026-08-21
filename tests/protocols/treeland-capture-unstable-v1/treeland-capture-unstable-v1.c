// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-capture-unstable-v1.h"
#include "treeland-capture-unstable-v1-client-protocol.h"

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

static void context_source_ready(void *data, struct treeland_capture_context_v1 *context,
                                 int32_t region_x, int32_t region_y,
                                 uint32_t region_width, uint32_t region_height,
                                 uint32_t source_type)
{
    (void)data;
    (void)context;
    (void)region_x;
    (void)region_y;
    (void)region_width;
    (void)region_height;
    (void)source_type;
}

static void context_a_source_failed(void *data, struct treeland_capture_context_v1 *context,
                                    uint32_t reason)
{
    (void)context;
    (void)reason;
    ((struct test_ctx *)data)->a_source_failed_received = 1;
}

static void context_b_source_failed(void *data, struct treeland_capture_context_v1 *context,
                                    uint32_t reason)
{
    (void)context;
    struct test_ctx *ctx = data;
    ctx->b_source_failed_received = 1;
    ctx->b_source_failed_reason = reason;
}

static void context_c_source_failed(void *data, struct treeland_capture_context_v1 *context,
                                    uint32_t reason)
{
    (void)context;
    (void)reason;
    ((struct test_ctx *)data)->c_source_failed_received = 1;
}

static const struct treeland_capture_context_v1_listener context_listener_a = {
    .source_ready = context_source_ready,
    .source_failed = context_a_source_failed,
};

static const struct treeland_capture_context_v1_listener context_listener_b = {
    .source_ready = context_source_ready,
    .source_failed = context_b_source_failed,
};

static const struct treeland_capture_context_v1_listener context_listener_c = {
    .source_ready = context_source_ready,
    .source_failed = context_c_source_failed,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->manager = client_bind(&ctx->connection, "treeland_capture_manager_v1",
                                      &treeland_capture_manager_v1_interface, 1);
    return ctx->manager != NULL;
}

static int get_context(struct test_ctx *ctx)
{
    ctx->context_a = treeland_capture_manager_v1_get_context(ctx->manager);
    if (ctx->context_a)
        treeland_capture_context_v1_add_listener(ctx->context_a, &context_listener_a, ctx);
    return ctx->context_a != NULL;
}

static int select_source(struct test_ctx *ctx)
{

    treeland_capture_context_v1_select_source(ctx->context_a,
                                              TREELAND_CAPTURE_CONTEXT_V1_SOURCE_TYPE_OUTPUT,
                                              0, 0, NULL);
    return 1;
}

static int select_source_busy(struct test_ctx *ctx)
{

    ctx->context_b = treeland_capture_manager_v1_get_context(ctx->manager);
    if (!ctx->context_b)
        return 0;
    treeland_capture_context_v1_add_listener(ctx->context_b, &context_listener_b, ctx);
    treeland_capture_context_v1_select_source(ctx->context_b,
                                              TREELAND_CAPTURE_CONTEXT_V1_SOURCE_TYPE_WINDOW,
                                              0, 0, NULL);
    return 1;
}

static int source_failed_busy_received(struct test_ctx *ctx)
{
    return ctx->b_source_failed_received
           && ctx->b_source_failed_reason == TREELAND_CAPTURE_CONTEXT_V1_SOURCE_FAILURE_SELECTOR_BUSY;
}

static int no_source_failed_on_first_select(struct test_ctx *ctx)
{
    return ctx->a_source_failed_received == 0;
}

static int destroy_in_selection(struct test_ctx *ctx)
{

    treeland_capture_context_v1_destroy(ctx->context_a);
    ctx->context_a = NULL;
    return 1;
}

static int select_source_after_clear(struct test_ctx *ctx)
{
    ctx->context_c = treeland_capture_manager_v1_get_context(ctx->manager);
    if (!ctx->context_c)
        return 0;
    treeland_capture_context_v1_add_listener(ctx->context_c, &context_listener_c, ctx);
    treeland_capture_context_v1_select_source(ctx->context_c,
                                              TREELAND_CAPTURE_CONTEXT_V1_SOURCE_TYPE_REGION,
                                              0, 0, NULL);
    return 1;
}

static int no_source_failed_after_clear(struct test_ctx *ctx)
{
    return ctx->c_source_failed_received == 0;
}

static struct treeland_capture_manager_v1 *connect_fresh_manager(
    const char *socket_name, struct client_connection *connection)
{
    if (!client_connect(connection, socket_name))
        return NULL;
    return client_bind(connection, "treeland_capture_manager_v1",
                              &treeland_capture_manager_v1_interface, 1);
}

static int capture_without_source_errors(struct test_ctx *ctx)
{

    struct client_connection connection;
    struct treeland_capture_manager_v1 *manager =
        connect_fresh_manager(ctx->socket_name, &connection);
    if (!manager)
        return 0;
    struct treeland_capture_context_v1 *context = treeland_capture_manager_v1_get_context(manager);
    treeland_capture_context_v1_capture(context);
    const struct wl_interface *iface;
    uint32_t code;
    const int errored = wl_display_roundtrip(connection.display) < 0
                        && wl_display_get_protocol_error(connection.display, &iface, &code)
                               == WL_DISPLAY_ERROR_IMPLEMENTATION;
    client_disconnect(&connection);
    return errored;
}

static int create_session_without_source_errors(struct test_ctx *ctx)
{

    struct client_connection connection;
    struct treeland_capture_manager_v1 *manager =
        connect_fresh_manager(ctx->socket_name, &connection);
    if (!manager)
        return 0;
    struct treeland_capture_context_v1 *context = treeland_capture_manager_v1_get_context(manager);
    treeland_capture_context_v1_create_session(context);
    const struct wl_interface *iface;
    uint32_t code;
    const int errored = wl_display_roundtrip(connection.display) < 0
                        && wl_display_get_protocol_error(connection.display, &iface, &code)
                               == WL_DISPLAY_ERROR_IMPLEMENTATION;
    client_disconnect(&connection);
    return errored;
}

static const struct test_case cases[] = {
    { "manager.get_context", get_context },
    { "context.select_source", select_source },
    { "context.select_source_busy", select_source_busy },
    { "context.event.source_failed_busy", source_failed_busy_received },
    { "context.event.no_source_failed_on_first_select", no_source_failed_on_first_select },
    { "context.destroy_in_selection", destroy_in_selection },
    { "context.select_source_after_clear", select_source_after_clear },
    { "context.event.no_source_failed_after_clear", no_source_failed_after_clear },
    { "context.capture_without_source_errors", capture_without_source_errors },
    { "context.create_session_without_source_errors", create_session_without_source_errors },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->context_c) treeland_capture_context_v1_destroy(ctx->context_c);
    if (ctx->context_b) treeland_capture_context_v1_destroy(ctx->context_b);
    if (ctx->context_a) treeland_capture_context_v1_destroy(ctx->context_a);
    if (ctx->manager) treeland_capture_manager_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    ctx.socket_name = socket_name;
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_capture_manager_v1\n");
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
