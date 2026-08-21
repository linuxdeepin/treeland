// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-wallpaper-color-v1.h"
#include "server-bridge-api.h"
#include "treeland-wallpaper-color-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void wallpaper_color_set_color(const char *output, int is_dark);

struct wallpaper_color_update_args {
    const char *output;
    int is_dark;
};

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

static void output_color(void *data, struct treeland_wallpaper_color_manager_v1 *manager,
                         const char *output, uint32_t isdark)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->output_color_count++;
    ctx->output_color_isdark = (int)isdark;
    snprintf(ctx->output_color_name, sizeof(ctx->output_color_name), "%s", output);
}

static const struct treeland_wallpaper_color_manager_v1_listener manager_listener = {
    .output_color = output_color,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->manager = client_bind(&ctx->connection, "treeland_wallpaper_color_manager_v1",
                                      &treeland_wallpaper_color_manager_v1_interface, 1);
    if (!ctx->manager)
        return 0;
    treeland_wallpaper_color_manager_v1_add_listener(ctx->manager, &manager_listener, ctx);
    return 1;
}

static void set_color_callback(void *data)
{
    struct wallpaper_color_update_args *args = data;
    wallpaper_color_set_color(args->output, args->is_dark);
}

static int set_color(struct test_ctx *ctx, const char *output, int is_dark)
{
    (void)ctx;
    struct wallpaper_color_update_args args = { .output = output, .is_dark = is_dark };
    return invoke_on_server_thread(set_color_callback, &args);
}

static int set_color_dark(struct test_ctx *ctx) { return set_color(ctx, "test-output", 1); }
static int set_color_light(struct test_ctx *ctx) { return set_color(ctx, "test-output", 0); }
static int watch(struct test_ctx *ctx)
{
    treeland_wallpaper_color_manager_v1_watch(ctx->manager, "test-output");
    return 1;
}
static int watch_unknown(struct test_ctx *ctx)
{
    treeland_wallpaper_color_manager_v1_watch(ctx->manager, "never-set-output");
    return 1;
}
static int unwatch(struct test_ctx *ctx)
{
    treeland_wallpaper_color_manager_v1_unwatch(ctx->manager, "test-output");
    return 1;
}
static int immediate_event_received(struct test_ctx *ctx)
{
    return ctx->output_color_count == 1 && ctx->output_color_isdark == 1 &&
           strcmp(ctx->output_color_name, "test-output") == 0;
}
static int no_duplicate_event(struct test_ctx *ctx) { return ctx->output_color_count == 1; }
static int changed_event_received(struct test_ctx *ctx)
{
    return ctx->output_color_count == 2 && ctx->output_color_isdark == 0 &&
           strcmp(ctx->output_color_name, "test-output") == 0;
}
static int unknown_output_ignored(struct test_ctx *ctx) { return ctx->output_color_count == 2; }
static int no_event_after_unwatch(struct test_ctx *ctx) { return ctx->output_color_count == 2; }

static const struct test_case cases[] = {
    { "server.set_color_dark", set_color_dark },
    { "manager.watch", watch },
    { "event.output_color.immediate", immediate_event_received },
    { "server.set_color_same_value", set_color_dark },
    { "event.no_duplicate_same_color", no_duplicate_event },
    { "server.set_color_light", set_color_light },
    { "event.output_color.changed", changed_event_received },
    { "manager.watch_unknown_output", watch_unknown },
    { "event.ignored_unknown_output", unknown_output_ignored },
    { "manager.unwatch", unwatch },
    { "server.set_color_after_unwatch", set_color_dark },
    { "event.no_event_after_unwatch", no_event_after_unwatch },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->manager)
        treeland_wallpaper_color_manager_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_wallpaper_color_manager_v1\n");
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
