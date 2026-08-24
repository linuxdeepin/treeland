// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-app-id-resolver-v1.h"
#include "server-bridge-api.h"
#include "treeland-app-id-resolver-v1-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXPECTED_APP_ID "org.deepin.dde.test-app"
#define EXPECTED_SANDBOX "test-sandbox"

struct app_id_resolver_test_state {
    int resolve_started;
    int resolve_matched;
    int resolve_empty;
};

extern struct app_id_resolver_test_state g_app_id_resolver_snapshot;
extern void server_start_resolve(void *data);
extern void server_snapshot_state(void *data);

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
};

void test_init(struct test_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->identify_pidfd = -1;
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

static void identify_request(void *data, struct treeland_app_id_resolver_v1 *resolver,
                             uint32_t request_id, int32_t pidfd)
{
    (void)resolver;
    struct test_ctx *ctx = data;
    ctx->identify_received = 1;
    ctx->identify_request_id = request_id;
    ctx->identify_pidfd = pidfd;
}

static const struct treeland_app_id_resolver_v1_listener resolver_listener = {
    .identify_request = identify_request,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->manager = client_bind(&ctx->connection, "treeland_app_id_resolver_manager_v1",
                                      &treeland_app_id_resolver_manager_v1_interface, 1);
    return ctx->manager != NULL;
}

static int pidfd_refers_to_self(int pidfd)
{
    char path[64];
    char line[128];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", pidfd);
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    int target_pid = -1;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "Pid:\t%d", &target_pid) == 1)
            break;
    }
    fclose(file);
    return target_pid == (int)getpid();
}

static int resolve_without_resolver_fails(struct test_ctx *ctx)
{
    (void)ctx;
    if (!invoke_on_server_thread(server_start_resolve, NULL))
        return 0;
    if (!invoke_on_server_thread(server_snapshot_state, NULL))
        return 0;
    return g_app_id_resolver_snapshot.resolve_started == 0;
}

static int get_resolver_creates_object(struct test_ctx *ctx)
{
    ctx->resolver = treeland_app_id_resolver_manager_v1_get_resolver(ctx->manager);
    if (!ctx->resolver)
        return 0;
    return treeland_app_id_resolver_v1_add_listener(ctx->resolver, &resolver_listener, ctx) == 0;
}

static int resolve_roundtrip(struct test_ctx *ctx)
{
    ctx->identify_received = 0;
    ctx->identify_pidfd = -1;
    if (!invoke_on_server_thread(server_start_resolve, NULL))
        return 0;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;

    const int pidfd_ok = ctx->identify_received
                      && ctx->identify_request_id == 1
                      && ctx->identify_pidfd >= 0
                      && fcntl(ctx->identify_pidfd, F_GETFD) >= 0
                      && pidfd_refers_to_self(ctx->identify_pidfd);
    if (ctx->identify_pidfd >= 0) {
        close(ctx->identify_pidfd);
        ctx->identify_pidfd = -1;
    }
    if (!pidfd_ok)
        return 0;

    treeland_app_id_resolver_v1_respond(ctx->resolver, ctx->identify_request_id,
                                        EXPECTED_APP_ID, EXPECTED_SANDBOX);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!invoke_on_server_thread(server_snapshot_state, NULL))
        return 0;
    return g_app_id_resolver_snapshot.resolve_started == 1
        && g_app_id_resolver_snapshot.resolve_matched == 1;
}

static int second_resolve_empty_response(struct test_ctx *ctx)
{
    ctx->identify_received = 0;
    ctx->identify_pidfd = -1;
    if (!invoke_on_server_thread(server_start_resolve, NULL))
        return 0;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;

    const int pidfd_ok = ctx->identify_received
                      && ctx->identify_request_id == 2
                      && ctx->identify_pidfd >= 0
                      && fcntl(ctx->identify_pidfd, F_GETFD) >= 0;
    if (ctx->identify_pidfd >= 0) {
        close(ctx->identify_pidfd);
        ctx->identify_pidfd = -1;
    }
    if (!pidfd_ok)
        return 0;

    treeland_app_id_resolver_v1_respond(ctx->resolver, ctx->identify_request_id, "",
                                        EXPECTED_SANDBOX);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!invoke_on_server_thread(server_snapshot_state, NULL))
        return 0;
    return g_app_id_resolver_snapshot.resolve_started == 1
        && g_app_id_resolver_snapshot.resolve_empty == 1;
}

static int duplicate_get_resolver_errors(struct test_ctx *ctx)
{

    ctx->display_errored = 1;

    (void)treeland_app_id_resolver_manager_v1_get_resolver(ctx->manager);

    if (wl_display_roundtrip(ctx->display) >= 0)
        return 0;
    if (wl_display_get_error(ctx->display) != EPROTO)
        return 0;

    const struct wl_interface *error_interface = NULL;
    uint32_t error_id = 0;
    const uint32_t code = wl_display_get_protocol_error(ctx->display, &error_interface, &error_id);
    if (code != WL_DISPLAY_ERROR_INVALID_OBJECT)
        return 0;
    if (error_interface != &treeland_app_id_resolver_manager_v1_interface)
        return 0;
    if (error_id != wl_proxy_get_id((struct wl_proxy *)ctx->manager))
        return 0;
    return 1;
}

static const struct test_case cases[] = {
    { "resolve_without_resolver_fails", resolve_without_resolver_fails },
    { "get_resolver_creates_object", get_resolver_creates_object },
    { "resolve_roundtrip", resolve_roundtrip },
    { "second_resolve_empty_response", second_resolve_empty_response },
    { "duplicate_get_resolver_errors", duplicate_get_resolver_errors },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->resolver)
        treeland_app_id_resolver_v1_destroy(ctx->resolver);
    if (ctx->manager)
        treeland_app_id_resolver_manager_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_app_id_resolver_manager_v1\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");
        if (ctx.display_errored)
            continue;
        if (wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
