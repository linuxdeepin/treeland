// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-xwindow-control-unstable-v1.h"
#include "server-bridge-api.h"
#include "treeland-xwindow-control-unstable-v1-client-protocol.h"

#include <stdarg.h>
#include <stdint.h>
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

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_xwindow_control_v1",
                                      &treeland_xwindow_control_v1_interface, 1);
    return ctx->manager != NULL;
}

static int bind_manager(struct test_ctx *ctx)
{
    return ctx->manager != NULL;
}

static void callback_done(void *data, struct wl_callback *callback, uint32_t result)
{
    (void)callback;
    struct test_ctx *ctx = data;
    ctx->callback_received = 1;
    ctx->callback_result = result;
}

static const struct wl_callback_listener callback_listener = {
    .done = callback_done,
};

static int set_xwindow_position_relative(struct test_ctx *ctx)
{
    if (!ctx->compositor || !ctx->manager)
        return 0;
    struct wl_surface *surface = wl_compositor_create_surface(ctx->compositor);
    if (!surface)
        return 0;
    wl_fixed_t dx = wl_fixed_from_int(0);
    wl_fixed_t dy = wl_fixed_from_int(0);
    struct wl_callback *cb =
        treeland_xwindow_control_v1_set_xwindow_position_relative(ctx->manager, 0, surface, dx, dy);
    if (!cb)
        return 0;
    wl_callback_add_listener(cb, &callback_listener, ctx);
    wl_surface_destroy(surface);
    return 1;
}

static int callback_failure_received(struct test_ctx *ctx)
{
    return ctx->callback_received && ctx->callback_result == 1;
}

static int destroy_manager(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    treeland_xwindow_control_v1_destroy(ctx->manager);
    ctx->manager = NULL;
    return 1;
}

static const struct test_case cases[] = {
    { "manager.bind", bind_manager },
    { "request.set_xwindow_position_relative", set_xwindow_position_relative },
    { "callback.done(failure)", callback_failure_received },
    { "manager.destroy", destroy_manager },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->manager) treeland_xwindow_control_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_xwindow_control_v1\n");
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
