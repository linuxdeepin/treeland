// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-screensaver-v1.h"
#include "server-bridge-api.h"
#include "treeland-screensaver-v1-client-protocol.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void screensaver_query_inhibited(void *data);

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
};

void test_init(struct test_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->result_cap = 16;
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
    strncpy(ctx->socket_name, socket_name, sizeof(ctx->socket_name) - 1);
    ctx->socket_name[sizeof(ctx->socket_name) - 1] = '\0';
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->screensaver = client_bind(&ctx->connection, "treeland_screensaver_v1",
                                          &treeland_screensaver_v1_interface, 2);
    return ctx->screensaver != NULL;
}

static int bind_max_version(struct test_ctx *ctx)
{

    return treeland_screensaver_v1_get_version(ctx->screensaver) == 1;
}

static int inhibit(struct test_ctx *ctx)
{
    treeland_screensaver_v1_inhibit(ctx->screensaver, "protocol-test", "test inhibit");
    return 1;
}

static int server_inhibited_after_inhibit(struct test_ctx *ctx)
{
    (void)ctx;
    int inhibited = 0;
    if (!invoke_on_server_thread(screensaver_query_inhibited, &inhibited))
        return 0;
    return inhibited == 1;
}

static int uninhibit(struct test_ctx *ctx)
{
    treeland_screensaver_v1_uninhibit(ctx->screensaver);
    return 1;
}

static int server_inhibited_after_uninhibit(struct test_ctx *ctx)
{
    (void)ctx;
    int inhibited = 1;
    if (!invoke_on_server_thread(screensaver_query_inhibited, &inhibited))
        return 0;
    return inhibited == 0;
}

static int expect_protocol_error(struct test_ctx *ctx, struct wl_display *display,
                                 uint32_t expected_code)
{

    if (wl_display_roundtrip(display) >= 0)
        return 0;
    ctx->roundtripped = 1;
    if (errno != EPROTO)
        return 0;
    const struct wl_interface *interface = NULL;
    uint32_t id = 0;
    const uint32_t code = wl_display_get_protocol_error(display, &interface, &id);
    return code == expected_code && interface == &treeland_screensaver_v1_interface;
}

static int uninhibit_without_inhibit(struct test_ctx *ctx)
{

    treeland_screensaver_v1_uninhibit(ctx->screensaver);
    return expect_protocol_error(ctx, ctx->display, TREELAND_SCREENSAVER_V1_ERROR_NOT_YET_INHIBITED);
}

static int inhibit_twice(struct test_ctx *ctx)
{

    if (!client_connect(&ctx->error_connection, ctx->socket_name))
        return 0;
    ctx->error_screensaver = client_bind(&ctx->error_connection, "treeland_screensaver_v1",
                                                &treeland_screensaver_v1_interface, 2);
    if (!ctx->error_screensaver)
        return 0;
    treeland_screensaver_v1_inhibit(ctx->error_screensaver, "protocol-test", "first inhibit");
    treeland_screensaver_v1_inhibit(ctx->error_screensaver, "protocol-test", "second inhibit");
    return expect_protocol_error(ctx, ctx->error_connection.display,
                                 TREELAND_SCREENSAVER_V1_ERROR_ALREADY_INHIBITED);
}

static const struct test_case cases[] = {
    { "bind.max_version", bind_max_version },
    { "inhibit", inhibit },
    { "server.inhibited_after_inhibit", server_inhibited_after_inhibit },
    { "uninhibit", uninhibit },
    { "server.inhibited_after_uninhibit", server_inhibited_after_uninhibit },
    { "uninhibit_without_inhibit.error.not_yet_inhibited", uninhibit_without_inhibit },
    { "inhibit_twice.error.already_inhibited", inhibit_twice },
};

void test_cleanup(struct test_ctx *ctx)
{

    client_disconnect(&ctx->error_connection);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_screensaver_v1\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    const size_t case_count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < case_count; ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");

        if (!ctx.roundtripped && wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
        ctx.roundtripped = 0;
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
