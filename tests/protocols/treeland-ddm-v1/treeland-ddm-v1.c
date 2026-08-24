// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-ddm-v1.h"
#include "server-bridge-api.h"
#include "treeland-ddm-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ddm_check_is_connected(void *data);

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

static void switch_to_vt(void *data, struct treeland_ddm_v1 *ddm, int32_t vtnr)
{
    (void)ddm;
    (void)vtnr;
    ((struct test_ctx *)data)->switch_to_vt_received = 1;
}

static void acquire_vt(void *data, struct treeland_ddm_v1 *ddm, int32_t vtnr)
{
    (void)ddm;
    (void)vtnr;
    ((struct test_ctx *)data)->acquire_vt_received = 1;
}

static const struct treeland_ddm_v1_listener ddm_listener = {
    .switch_to_vt = switch_to_vt,
    .acquire_vt = acquire_vt,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->socket_name = socket_name;
    return 1;
}

static int registry_has_ddm(struct test_ctx *ctx)
{
    for (uint32_t i = 0; i < ctx->connection.global_count; ++i) {
        if (strcmp(ctx->connection.globals[i].interface, "treeland_ddm_v1") == 0)
            return 1;
    }
    return 0;
}

static int registry_advertises_version_1(struct test_ctx *ctx)
{
    for (uint32_t i = 0; i < ctx->connection.global_count; ++i) {
        if (strcmp(ctx->connection.globals[i].interface, "treeland_ddm_v1") == 0)
            return ctx->connection.globals[i].version == 1;
    }
    return 0;
}

static int bind_requesting_v2_clamps_to_v1(struct test_ctx *ctx)
{

    struct treeland_ddm_v1 *ddm = client_bind(&ctx->connection, "treeland_ddm_v1",
                                                     &treeland_ddm_v1_interface, 2);
    if (!ddm)
        return 0;
    const int version = (int)treeland_ddm_v1_get_version(ddm);
    wl_proxy_destroy((struct wl_proxy *)ddm);
    return version == 1;
}

static int bind_at_advertised_v1(struct test_ctx *ctx)
{
    ctx->ddm = client_bind(&ctx->connection, "treeland_ddm_v1",
                                  &treeland_ddm_v1_interface, 1);
    if (!ctx->ddm)
        return 0;
    ctx->bound_version = (int)treeland_ddm_v1_get_version(ctx->ddm);
    treeland_ddm_v1_add_listener(ctx->ddm, &ddm_listener, ctx);
    return ctx->bound_version == 1;
}

static int check_is_connected(struct test_ctx *ctx)
{
    (void)ctx;
    int connected = 0;
    if (!invoke_on_server_thread(ddm_check_is_connected, &connected))
        return 0;
    return connected;
}

static int is_connected_with_one_client(struct test_ctx *ctx)
{
    return check_is_connected(ctx);
}

static int second_client_keeps_connected(struct test_ctx *ctx)
{
    if (!client_connect(&ctx->aux, ctx->socket_name))
        return 0;
    struct treeland_ddm_v1 *aux_ddm = client_bind(&ctx->aux, "treeland_ddm_v1",
                                                         &treeland_ddm_v1_interface, 1);
    if (!aux_ddm)
        return 0;

    if (wl_display_roundtrip(ctx->aux.display) < 0)
        return 0;
    return check_is_connected(ctx);
}

static int no_events_received(struct test_ctx *ctx)
{
    return !ctx->switch_to_vt_received && !ctx->acquire_vt_received;
}

static int is_connected_false_after_all_clients_gone(struct test_ctx *ctx)
{

    if (ctx->ddm) {
        wl_proxy_destroy((struct wl_proxy *)ctx->ddm);
        ctx->ddm = NULL;
    }
    client_disconnect(&ctx->connection);
    ctx->display = NULL;
    client_disconnect(&ctx->aux);

    if (!client_connect(&ctx->checker, ctx->socket_name))
        return 0;
    const int connected = check_is_connected(ctx);
    client_disconnect(&ctx->checker);
    return !connected;
}

static const struct test_case cases[] = {
    { "registry.advertises_treeland_ddm_v1", registry_has_ddm },
    { "registry.advertises_version_1", registry_advertises_version_1 },
    { "bind.requesting_v2_clamps_to_advertised_v1", bind_requesting_v2_clamps_to_v1 },
    { "bind.at_advertised_v1", bind_at_advertised_v1 },
    { "manager.isConnected_with_one_client", is_connected_with_one_client },
    { "manager.isConnected_with_two_clients", second_client_keeps_connected },
    { "events.none_after_bind", no_events_received },
    { "manager.isConnected_false_after_all_clients_gone", is_connected_false_after_all_clients_gone },
};

void test_cleanup(struct test_ctx *ctx)
{

    if (ctx->ddm)
        wl_proxy_destroy((struct wl_proxy *)ctx->ddm);
    client_disconnect(&ctx->checker);
    client_disconnect(&ctx->aux);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to treeland_ddm_v1 test server\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");

        if (ctx.display && wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
