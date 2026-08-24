// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-window-management-v1.h"
#include "server-bridge-api.h"
#include "treeland-window-management-v1-client-protocol.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void window_management_get_desktop_state(void *data);
extern void window_management_set_desktop_state(void *data);

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

static void show_desktop(void *data, struct treeland_window_management_v1 *manager, uint32_t state)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->show_desktop_received = 1;
    ctx->show_desktop_last_state = state;
    ++ctx->show_desktop_count;
}

static const struct treeland_window_management_v1_listener manager_listener = {
    .show_desktop = show_desktop,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->manager = client_bind(&ctx->connection, "treeland_window_management_v1",
                                      &treeland_window_management_v1_interface, 1);
    if (ctx->manager)
        treeland_window_management_v1_add_listener(ctx->manager, &manager_listener, ctx);
    return ctx->manager != NULL;
}

static int bind_manager(struct test_ctx *ctx) { return ctx->manager != NULL; }

static int initial_state_event(struct test_ctx *ctx)
{

    return ctx->show_desktop_received
        && ctx->show_desktop_last_state == TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL;
}

static int set_desktop_show(struct test_ctx *ctx)
{
    treeland_window_management_v1_set_desktop(ctx->manager, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW);
    return 1;
}

static int set_desktop_normal(struct test_ctx *ctx)
{
    treeland_window_management_v1_set_desktop(ctx->manager, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL);
    return 1;
}

static int set_desktop_preview(struct test_ctx *ctx)
{
    treeland_window_management_v1_set_desktop(ctx->manager, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_PREVIEW_SHOW);
    return 1;
}

static int desktop_state_is(struct test_ctx *ctx, uint32_t expected)
{
    uint32_t server_state = UINT32_MAX;
    if (!invoke_on_server_thread(window_management_get_desktop_state, &server_state))
        return 0;
    return server_state == expected && ctx->show_desktop_last_state == expected;
}

static int desktop_state_show(struct test_ctx *ctx)
{
    return desktop_state_is(ctx, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW);
}

static int desktop_state_normal(struct test_ctx *ctx)
{
    return desktop_state_is(ctx, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL);
}

static int desktop_state_preview(struct test_ctx *ctx)
{
    return desktop_state_is(ctx, TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_PREVIEW_SHOW);
}

static int server_set_desktop_state(struct test_ctx *ctx)
{
    (void)ctx;
    uint32_t state = TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW;
    return invoke_on_server_thread(window_management_set_desktop_state, &state);
}

static int server_event_received(struct test_ctx *ctx)
{

    return ctx->show_desktop_count == 5
        && ctx->show_desktop_last_state == TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW;
}

static int destroy_manager(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    treeland_window_management_v1_destroy(ctx->manager);
    ctx->manager = NULL;
    return 1;
}

static const struct test_case cases[] = {
    { "manager.bind", bind_manager },
    { "manager.event.show_desktop.initial", initial_state_event },
    { "manager.set_desktop(show)", set_desktop_show },
    { "desktop_state.show", desktop_state_show },
    { "manager.set_desktop(normal)", set_desktop_normal },
    { "desktop_state.normal", desktop_state_normal },
    { "manager.set_desktop(preview_show)", set_desktop_preview },
    { "desktop_state.preview_show", desktop_state_preview },
    { "server.set_desktop_state(show)", server_set_desktop_state },
    { "server.event.show_desktop", server_event_received },
    { "manager.destroy", destroy_manager },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->manager) treeland_window_management_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_window_management_v1\n");
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
