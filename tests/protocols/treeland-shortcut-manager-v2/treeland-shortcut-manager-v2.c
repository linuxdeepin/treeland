// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-shortcut-manager-v2.h"
#include "treeland-shortcut-manager-v2-client-protocol.h"

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

static void commit_success(void *data, struct treeland_shortcut_manager_v2 *manager)
{
    (void)manager;
    ((struct test_ctx *)data)->commit_success_received = 1;
}

static void commit_failure(void *data, struct treeland_shortcut_manager_v2 *manager,
                           const char *name, uint32_t error)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->commit_failure_received = 1;
    ctx->commit_failure_error = error;
    if (name) {
        strncpy(ctx->commit_failure_name, name, sizeof(ctx->commit_failure_name) - 1);
        ctx->commit_failure_name[sizeof(ctx->commit_failure_name) - 1] = '\0';
    }
}

static const struct treeland_shortcut_manager_v2_listener manager_listener = {
    .activated = NULL,
    .commit_success = commit_success,
    .commit_failure = commit_failure,
};

static void capture_captured(void *data, struct treeland_shortcut_capture_v1 *capture,
                             const char *key)
{
    (void)capture;
    struct test_ctx *ctx = data;
    ctx->capture_captured_received = 1;
    if (key) {
        strncpy(ctx->capture_captured_key, key, sizeof(ctx->capture_captured_key) - 1);
        ctx->capture_captured_key[sizeof(ctx->capture_captured_key) - 1] = '\0';
    }
}

static void capture_failed(void *data, struct treeland_shortcut_capture_v1 *capture,
                           uint32_t reason)
{
    (void)capture;
    struct test_ctx *ctx = data;
    ctx->capture_failed_received = 1;
    ctx->capture_failed_reason = reason;
}

static const struct treeland_shortcut_capture_v1_listener capture_listener = {
    .captured = capture_captured,
    .failed = capture_failed,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_shortcut_manager_v2",
                                      &treeland_shortcut_manager_v2_interface, 2);
    if (!ctx->manager)
        return 0;
    treeland_shortcut_manager_v2_add_listener(ctx->manager, &manager_listener, ctx);
    return 1;
}

static int bind_manager(struct test_ctx *ctx) { return ctx->manager != NULL; }

static int acquire(struct test_ctx *ctx)
{
    treeland_shortcut_manager_v2_acquire(ctx->manager);
    return 1;
}

static int commit_empty(struct test_ctx *ctx)
{
    ctx->commit_success_received = 0;
    treeland_shortcut_manager_v2_commit(ctx->manager);
    return 1;
}

static int commit_success_received(struct test_ctx *ctx) { return ctx->commit_success_received; }

static int bind_key_commit(struct test_ctx *ctx)
{
    ctx->commit_success_received = 0;
    treeland_shortcut_manager_v2_bind_key(ctx->manager, "test-shortcut", "Ctrl+Alt+T",
                                          TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_FLAG_KEY_PRESS
                                              | TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_FLAG_REPEAT,
                                          TREELAND_SHORTCUT_MANAGER_V2_ACTION_NOTIFY);
    treeland_shortcut_manager_v2_commit(ctx->manager);
    return 1;
}

static int commit_success_after_bind(struct test_ctx *ctx)
{
    return ctx->commit_success_received;
}

static int commit_again(struct test_ctx *ctx)
{
    ctx->commit_success_received = 0;
    treeland_shortcut_manager_v2_commit(ctx->manager);
    return 1;
}

static int capture_request(struct test_ctx *ctx)
{
    ctx->capture_failed_received = 0;
    ctx->capture_captured_received = 0;
    if (!ctx->compositor)
        return 0;
    ctx->test_surface = wl_compositor_create_surface(ctx->compositor);
    if (!ctx->test_surface)
        return 0;
    ctx->capture = treeland_shortcut_manager_v2_capture_next_shortcut(ctx->manager,
                                                                      ctx->test_surface, NULL);
    if (!ctx->capture)
        return 0;
    treeland_shortcut_capture_v1_add_listener(ctx->capture, &capture_listener, ctx);
    return 1;
}

static int capture_failed_not_active(struct test_ctx *ctx)
{

    return ctx->capture_failed_received
        && ctx->capture_failed_reason == TREELAND_SHORTCUT_CAPTURE_V1_FAILED_REASON_NOT_ACTIVE
        && !ctx->capture_captured_received;
}

static const struct test_case cases[] = {
    { "manager.bind", bind_manager },
    { "manager.acquire", acquire },
    { "manager.commit_empty", commit_empty },
    { "event.commit_success", commit_success_received },
    { "manager.bind_key_commit", bind_key_commit },
    { "event.commit_success_after_bind", commit_success_after_bind },
    { "manager.commit_again", commit_again },
    { "event.commit_success_again", commit_success_received },
    { "capture.request", capture_request },
    { "event.capture_failed_not_active", capture_failed_not_active },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->capture) treeland_shortcut_capture_v1_destroy(ctx->capture);
    if (ctx->test_surface) wl_surface_destroy(ctx->test_surface);
    if (ctx->manager) treeland_shortcut_manager_v2_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_shortcut_manager_v2\n");
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
