/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Pure-C Wayland client for the treeland-screensaver-v1 protocol test.
 */
#include "treeland-screensaver-v1.h"
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
    if (!protocol_test_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->screensaver = protocol_test_bind(&ctx->connection, "treeland_screensaver_v1",
                                          &treeland_screensaver_v1_interface, 2);
    return ctx->screensaver != NULL;
}

static int bind_max_version(struct test_ctx *ctx)
{
    /* The compositor implements the interface at version 1
     * (ScreensaverInterfaceV1::InterfaceVersion); protocol_test_bind caps the
     * requested version at the advertised one, so the destroy request
     * (since 2) is not available on this object. */
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
    if (!protocol_test_invoke_server(screensaver_query_inhibited, &inhibited))
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
    if (!protocol_test_invoke_server(screensaver_query_inhibited, &inhibited))
        return 0;
    return inhibited == 0;
}

/*
 * The module posts the error with wl_resource_post_error and the server then
 * closes the client connection; the roundtrip surfaces the protocol error on
 * the client side. Returns 1 when the error matches expected_code and was
 * raised on the treeland_screensaver_v1 object.
 */
static int expect_protocol_error(struct test_ctx *ctx, struct wl_display *display,
                                 uint32_t expected_code)
{
    /* This case performs its own roundtrip; the case loop must not roundtrip
     * again on the (possibly dead) connection. */
    if (wl_display_roundtrip(display) >= 0)
        return 0; /* the server should have posted an error and closed the connection */
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
    /* No inhibit is active on the primary connection (it was inhibited and
     * uninhibited in earlier cases). The module must post
     * TREELAND_SCREENSAVER_V1_ERROR_NOT_YET_INHIBITED, which kills this
     * connection, so this case performs its own roundtrip and is run last
     * on the primary connection. */
    treeland_screensaver_v1_uninhibit(ctx->screensaver);
    return expect_protocol_error(ctx, ctx->display, TREELAND_SCREENSAVER_V1_ERROR_NOT_YET_INHIBITED);
}

static int inhibit_twice(struct test_ctx *ctx)
{
    /* The primary connection died from the not_yet_inhibited error, so use a
     * fresh connection: the second inhibit must post
     * TREELAND_SCREENSAVER_V1_ERROR_ALREADY_INHIBITED. */
    if (!protocol_test_connect(&ctx->error_connection, ctx->socket_name))
        return 0;
    ctx->error_screensaver = protocol_test_bind(&ctx->error_connection, "treeland_screensaver_v1",
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
    /* The screensaver objects are not explicitly destroyed: the destroy
     * request is since=2 and the compositor implements the interface at
     * version 1. Disconnecting cleans up the client-side proxies; the server
     * releases the resources when the clients disconnect. */
    protocol_test_disconnect(&ctx->error_connection);
    protocol_test_disconnect(&ctx->connection);
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
        /* Error cases roundtrip internally (the roundtrip surfaces the posted
         * protocol error and the connection dies); skip the loop's roundtrip
         * for them so a dead connection is not counted as a failure. */
        if (!ctx.roundtripped && wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
        ctx.roundtripped = 0;
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
