/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Pure-C Wayland client for the treeland-wallpaper-shell-unstable-v1 protocol test.
 */
#include "treeland-wallpaper-shell-unstable-v1.h"
#include "treeland-wallpaper-shell-unstable-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void wallpaper_emit_play(void *data);
extern void wallpaper_emit_pause(void *data);
extern void wallpaper_emit_slow_down(void *data);
extern void wallpaper_query_failed(void *data);
extern void wallpaper_query_failed_count(void *data);
extern void wallpaper_query_ready_count(void *data);
extern void wallpaper_query_wallpaper_ready(void *data);
extern void wallpaper_query_produced(void *data);

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

static void surface_play(void *data, struct treeland_wallpaper_surface_v1 *surface)
{
    (void)surface;
    ((struct test_ctx *)data)->play_received = 1;
}

static void surface_pause(void *data, struct treeland_wallpaper_surface_v1 *surface)
{
    (void)surface;
    ((struct test_ctx *)data)->pause_received = 1;
}

static void surface_slow_down(void *data, struct treeland_wallpaper_surface_v1 *surface,
                              uint32_t duration)
{
    (void)surface;
    struct test_ctx *ctx = data;
    ctx->slow_down_received = 1;
    ctx->slow_down_duration = duration;
}

static const struct treeland_wallpaper_surface_v1_listener wallpaper_surface_listener = {
    .pause = surface_pause,
    .play = surface_play,
    .slow_down = surface_slow_down,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!protocol_test_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = protocol_test_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    /* treeland_wallpaper_shell_v1 is version 2; bind the full version so the
     * treeland_wallpaper_surface_v1.ready request (since 2) is dispatched. */
    ctx->shell = protocol_test_bind(&ctx->connection, "treeland_wallpaper_shell_v1",
                                    &treeland_wallpaper_shell_v1_interface, 2);
    return ctx->shell != NULL;
}

static int shell_bound_version(struct test_ctx *ctx)
{
    return treeland_wallpaper_shell_v1_get_version(ctx->shell) == 2;
}

static int create_wallpaper_surface(struct test_ctx *ctx)
{
    if (!ctx->compositor)
        return 0;
    ctx->test_surface = wl_compositor_create_surface(ctx->compositor);
    ctx->wallpaper_surface = treeland_wallpaper_shell_v1_get_treeland_wallpaper_surface(
        ctx->shell, ctx->test_surface, "/tmp/treeland-test-wallpaper.jpg");
    if (ctx->wallpaper_surface)
        treeland_wallpaper_surface_v1_add_listener(ctx->wallpaper_surface,
                                                   &wallpaper_surface_listener, ctx);
    return ctx->test_surface && ctx->wallpaper_surface;
}

static int produced_count_is_one(struct test_ctx *ctx)
{
    (void)ctx;
    int count = -1;
    return protocol_test_invoke_server(wallpaper_query_produced, &count) && count == 1;
}

static int source_failed(struct test_ctx *ctx)
{
    treeland_wallpaper_surface_v1_source_failed(
        ctx->wallpaper_surface, TREELAND_WALLPAPER_SURFACE_V1_ERROR_INVALID_SOURCE);
    return 1;
}

static int failed_signal_received(struct test_ctx *ctx)
{
    (void)ctx;
    int error = -1;
    int count = 0;
    return protocol_test_invoke_server(wallpaper_query_failed, &error)
        && protocol_test_invoke_server(wallpaper_query_failed_count, &count)
        && count == 1 && error == TREELAND_WALLPAPER_SURFACE_V1_ERROR_INVALID_SOURCE;
}

static int surface_ready(struct test_ctx *ctx)
{
    treeland_wallpaper_surface_v1_ready(ctx->wallpaper_surface);
    /* The surface has no buffer yet, so the compositor defers the ready
     * transition to the first wl_surface.commit; an empty commit triggers it. */
    wl_surface_commit(ctx->test_surface);
    return 1;
}

static int ready_signal_received(struct test_ctx *ctx)
{
    (void)ctx;
    int count = 0;
    int ready = 0;
    return protocol_test_invoke_server(wallpaper_query_ready_count, &count)
        && protocol_test_invoke_server(wallpaper_query_wallpaper_ready, &ready)
        && count == 1 && ready == 1;
}

static int emit_play(struct test_ctx *ctx) { (void)ctx; return protocol_test_invoke_server(wallpaper_emit_play, NULL); }
static int emit_pause(struct test_ctx *ctx) { (void)ctx; return protocol_test_invoke_server(wallpaper_emit_pause, NULL); }
static int emit_slow_down(struct test_ctx *ctx) { (void)ctx; return protocol_test_invoke_server(wallpaper_emit_slow_down, NULL); }
static int play_received(struct test_ctx *ctx) { return ctx->play_received; }
static int pause_received(struct test_ctx *ctx) { return ctx->pause_received; }
static int slow_down_received(struct test_ctx *ctx) { return ctx->slow_down_received && ctx->slow_down_duration == 500; }

static int destroy_wallpaper_surface(struct test_ctx *ctx)
{
    treeland_wallpaper_surface_v1_destroy(ctx->wallpaper_surface);
    ctx->wallpaper_surface = NULL;
    return 1;
}

static int produced_count_is_zero(struct test_ctx *ctx)
{
    (void)ctx;
    int count = -1;
    return protocol_test_invoke_server(wallpaper_query_produced, &count) && count == 0;
}

static const struct test_case cases[] = {
    { "bind.version", shell_bound_version },
    { "shell.get_treeland_wallpaper_surface", create_wallpaper_surface },
    { "shell.produced_wallpapers", produced_count_is_one },
    { "surface.source_failed", source_failed },
    { "surface.failed_signal", failed_signal_received },
    { "surface.ready", surface_ready },
    { "surface.ready_signal", ready_signal_received },
    { "server.emit_play", emit_play },
    { "surface.event.play", play_received },
    { "server.emit_pause", emit_pause },
    { "surface.event.pause", pause_received },
    { "server.emit_slow_down", emit_slow_down },
    { "surface.event.slow_down", slow_down_received },
    { "surface.destroy", destroy_wallpaper_surface },
    { "shell.produced_after_destroy", produced_count_is_zero },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->wallpaper_surface) treeland_wallpaper_surface_v1_destroy(ctx->wallpaper_surface);
    if (ctx->shell) treeland_wallpaper_shell_v1_destroy(ctx->shell);
    protocol_test_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_wallpaper_shell_v1\n");
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
