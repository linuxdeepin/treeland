// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "treeland-prelaunch-splash-v2.h"
#include "server-bridge-api.h"
#include "treeland-prelaunch-splash-v2-client-protocol.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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
    ctx->manager = client_bind(&ctx->connection, "treeland_prelaunch_splash_manager_v2",
                                      &treeland_prelaunch_splash_manager_v2_interface, 1);
    ctx->shm = client_bind(&ctx->connection, "wl_shm", &wl_shm_interface, 1);
    return ctx->manager != NULL;
}

static int bind_manager(struct test_ctx *ctx)
{
    return ctx->manager != NULL;
}

static int create_splash_null_icon(struct test_ctx *ctx)
{
    ctx->splash1 = treeland_prelaunch_splash_manager_v2_create_splash(
        ctx->manager, "test-app", "test-instance-1", "org.deepin.Sandbox", NULL);
    return ctx->splash1 != NULL;
}

static int create_splash_with_icon(struct test_ctx *ctx)
{

    if (!ctx->shm)
        return 0;
    const int stride = 4;
    const int size = stride;
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/treeland_splash_test_icon_%d", (int)getpid());
    const int fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(shm_name);
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return 0;
    }
    void *data = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return 0;
    }
    memset(data, 0, (size_t)size);
    struct wl_shm_pool *pool = wl_shm_create_pool(ctx->shm, fd, size);
    ctx->icon_buffer = wl_shm_pool_create_buffer(pool, 0, 1, 1, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, (size_t)size);
    close(fd);
    if (!ctx->icon_buffer)
        return 0;
    ctx->splash2 = treeland_prelaunch_splash_manager_v2_create_splash(
        ctx->manager, "test-app", "test-instance-2", "org.deepin.Sandbox", ctx->icon_buffer);
    return ctx->splash2 != NULL;
}

static int create_splash_same_app(struct test_ctx *ctx)
{
    ctx->splash3 = treeland_prelaunch_splash_manager_v2_create_splash(
        ctx->manager, "test-app", "test-instance-3", "org.deepin.Sandbox", NULL);
    return ctx->splash3 != NULL;
}

static int query_server(struct test_ctx *ctx)
{
    return invoke_on_server_thread(splash_query_state, &ctx->server);
}

static int splash_requested_null_icon(struct test_ctx *ctx)
{
    return ctx->server.request_count == 1
        && strcmp(ctx->server.requests[0].app_id, "test-app") == 0
        && strcmp(ctx->server.requests[0].instance_id, "test-instance-1") == 0
        && ctx->server.requests[0].icon_non_null == 0;
}

static int splash_requested_with_icon(struct test_ctx *ctx)
{
    return ctx->server.request_count == 2
        && strcmp(ctx->server.requests[1].app_id, "test-app") == 0
        && strcmp(ctx->server.requests[1].instance_id, "test-instance-2") == 0
        && ctx->server.requests[1].icon_non_null == 1;
}

static int splash_requested_no_dedup(struct test_ctx *ctx)
{

    return ctx->server.request_count == 3
        && strcmp(ctx->server.requests[2].app_id, "test-app") == 0
        && strcmp(ctx->server.requests[2].instance_id, "test-instance-3") == 0
        && ctx->server.requests[2].icon_non_null == 0;
}

static int destroy_splash(struct test_ctx *ctx)
{
    if (!ctx->splash1)
        return 0;
    treeland_prelaunch_splash_v2_destroy(ctx->splash1);
    ctx->splash1 = NULL;
    return 1;
}

static int splash_close_requested(struct test_ctx *ctx)
{
    return ctx->server.close_count == 1
        && strcmp(ctx->server.last_close_app_id, "test-app") == 0
        && strcmp(ctx->server.last_close_instance_id, "test-instance-1") == 0;
}

static int destroy_manager(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    treeland_prelaunch_splash_manager_v2_destroy(ctx->manager);
    ctx->manager = NULL;
    return 1;
}

static const struct test_case cases[] = {
    { "manager.bind", bind_manager },
    { "manager.create_splash_null_icon", create_splash_null_icon },
    { "server.splash_requested_null_icon", query_server },
    { "server.assert_splash_requested_null_icon", splash_requested_null_icon },
    { "manager.create_splash_with_icon", create_splash_with_icon },
    { "server.splash_requested_with_icon", query_server },
    { "server.assert_splash_requested_with_icon", splash_requested_with_icon },
    { "manager.create_splash_same_app", create_splash_same_app },
    { "server.splash_requested_same_app", query_server },
    { "server.assert_splash_requested_no_dedup", splash_requested_no_dedup },
    { "splash.destroy", destroy_splash },
    { "server.splash_close_requested", query_server },
    { "server.assert_splash_close_requested", splash_close_requested },
    { "manager.destroy", destroy_manager },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->splash3) treeland_prelaunch_splash_v2_destroy(ctx->splash3);
    if (ctx->splash2) treeland_prelaunch_splash_v2_destroy(ctx->splash2);
    if (ctx->splash1) treeland_prelaunch_splash_v2_destroy(ctx->splash1);
    if (ctx->icon_buffer) wl_buffer_destroy(ctx->icon_buffer);
    if (ctx->manager) treeland_prelaunch_splash_manager_v2_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_prelaunch_splash_manager_v2\n");
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
