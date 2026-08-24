// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-wallpaper-manager-unstable-v1.h"
#include "server-bridge-api.h"
#include "treeland-wallpaper-manager-unstable-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void wm_query_server_state(void *data);
extern void wm_emit_failed(void *data);
extern void wm_emit_changed(void *data);

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

static void wallpaper_failed(void *data, struct treeland_wallpaper_v1 *wallpaper,
                             const char *file_source, uint32_t error)
{
    (void)wallpaper;
    struct test_ctx *ctx = data;
    ctx->failed_received = 1;
    ctx->failed_error = (int)error;
    snprintf(ctx->failed_source, TEST_MSG_MAX, "%s", file_source ? file_source : "");
}

static void wallpaper_changed(void *data, struct treeland_wallpaper_v1 *wallpaper,
                              uint32_t role, uint32_t source_type, const char *file_source)
{
    (void)wallpaper;
    struct test_ctx *ctx = data;
    ctx->changed_received = 1;
    ctx->changed_role = (int)role;
    ctx->changed_type = (int)source_type;
    snprintf(ctx->changed_source, TEST_MSG_MAX, "%s", file_source ? file_source : "");
}

static const struct treeland_wallpaper_v1_listener wallpaper_listener = {
    .failed = wallpaper_failed,
    .changed = wallpaper_changed,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    ctx->output = client_bind(&ctx->connection, "wl_output", &wl_output_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_wallpaper_manager_v1",
                                      &treeland_wallpaper_manager_v1_interface, 1);
    return ctx->manager != NULL && ctx->output != NULL && ctx->compositor != NULL;
}

static int manager_get_wallpaper(struct test_ctx *ctx)
{
    if (!ctx->compositor || !ctx->output)
        return 0;
    ctx->test_surface = wl_compositor_create_surface(ctx->compositor);
    if (!ctx->test_surface)
        return 0;
    ctx->wallpaper = treeland_wallpaper_manager_v1_get_treeland_wallpaper(ctx->manager,
                                                                          ctx->output,
                                                                          ctx->test_surface);
    if (ctx->wallpaper)
        treeland_wallpaper_v1_add_listener(ctx->wallpaper, &wallpaper_listener, ctx);
    return ctx->wallpaper != NULL;
}

static int manager_get_wallpaper_null_surface(struct test_ctx *ctx)
{
    if (!ctx->output)
        return 0;
    ctx->wallpaper2 = treeland_wallpaper_manager_v1_get_treeland_wallpaper(ctx->manager,
                                                                           ctx->output,
                                                                           NULL);
    if (ctx->wallpaper2)
        treeland_wallpaper_v1_add_listener(ctx->wallpaper2, &wallpaper_listener, ctx);
    return ctx->wallpaper2 != NULL;
}

static int server_state_ok(struct test_ctx *ctx)
{
    (void)ctx;
    struct wm_server_state state;
    memset(&state, 0, sizeof(state));
    if (!invoke_on_server_thread(wm_query_server_state, &state))
        return 0;
    return state.wallpaper_created && state.second_created
        && state.output_valid && state.has_username;
}

static int emit_failed(struct test_ctx *ctx) { (void)ctx; return invoke_on_server_thread(wm_emit_failed, NULL); }
static int failed_event_ok(struct test_ctx *ctx)
{
    return ctx->failed_received
        && ctx->failed_error == TREELAND_WALLPAPER_V1_ERROR_INVALID_SOURCE
        && strcmp(ctx->failed_source, WM_TEST_SOURCE) == 0;
}

static int emit_changed(struct test_ctx *ctx) { (void)ctx; return invoke_on_server_thread(wm_emit_changed, NULL); }
static int changed_event_ok(struct test_ctx *ctx)
{
    return ctx->changed_received
        && ctx->changed_role == (TREELAND_WALLPAPER_V1_WALLPAPER_ROLE_DESKTOP
                                 | TREELAND_WALLPAPER_V1_WALLPAPER_ROLE_LOCKSCREEN)
        && ctx->changed_type == TREELAND_WALLPAPER_V1_WALLPAPER_SOURCE_TYPE_IMAGE
        && strcmp(ctx->changed_source, WM_TEST_SOURCE) == 0;
}

static const struct test_case cases[] = {
    { "manager.get_treeland_wallpaper", manager_get_wallpaper },
    { "manager.get_treeland_wallpaper_null_surface", manager_get_wallpaper_null_surface },
    { "server.state.wallpaper_created", server_state_ok },
    { "server.emit_failed", emit_failed },
    { "wallpaper.event.failed", failed_event_ok },
    { "server.emit_changed", emit_changed },
    { "wallpaper.event.changed", changed_event_ok },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->wallpaper2) treeland_wallpaper_v1_destroy(ctx->wallpaper2);
    if (ctx->wallpaper) treeland_wallpaper_v1_destroy(ctx->wallpaper);
    if (ctx->test_surface) wl_surface_destroy(ctx->test_surface);
    if (ctx->manager) treeland_wallpaper_manager_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect or bind treeland_wallpaper_manager_v1 "
                        "(need wl_compositor, wl_output and the manager global)\n");
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
