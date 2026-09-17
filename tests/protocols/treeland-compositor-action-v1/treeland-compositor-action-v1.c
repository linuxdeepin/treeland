// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-compositor-action-v1.h"
#include "server-bridge-api.h"
#include "treeland-compositor-action-unstable-v1-client-protocol.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int compositor_action_global_exists(void *data);
extern void compositor_action_get_workspace_index(void *data);
extern void compositor_action_get_show_desktop_state(void *data);

/* ShowDesktopInterfaceV1::State on the wire (Normal = 0, Show = 1) */
enum {
    PROD_DESKTOP_NORMAL = 0,
    PROD_DESKTOP_SHOW = 1,
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

static uint32_t production_workspace_index(struct test_ctx *ctx)
{
    (void)ctx;
    uint32_t index = UINT32_MAX;
    if (!invoke_on_server_thread(compositor_action_get_workspace_index, &index))
        return UINT32_MAX;
    return index;
}

static uint32_t production_show_desktop_state(struct test_ctx *ctx)
{
    (void)ctx;
    uint32_t state = UINT32_MAX;
    if (!invoke_on_server_thread(compositor_action_get_show_desktop_state, &state))
        return UINT32_MAX;
    return state;
}

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->action = client_bind(&ctx->connection, "treeland_compositor_action_v1",
                              &treeland_compositor_action_v1_interface, 1);
    return ctx->action != NULL;
}

static int bind_action(struct test_ctx *ctx) { return ctx->action != NULL; }

static int trigger(struct test_ctx *ctx, uint32_t action)
{
    treeland_compositor_action_v1_trigger(ctx->action, action);
    return 1;
}

static int trigger_workspace_2(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_WORKSPACE_2);
}

static int workspace_index_is_1(struct test_ctx *ctx)
{
    return production_workspace_index(ctx) == 1;
}

static int trigger_prev_workspace(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_PREV_WORKSPACE);
}

static int workspace_index_is_0(struct test_ctx *ctx)
{
    return production_workspace_index(ctx) == 0;
}

static int trigger_next_workspace(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_NEXT_WORKSPACE);
}

static int trigger_workspace_1(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_WORKSPACE_1);
}

static int trigger_workspace_12_out_of_range(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_WORKSPACE_12);
}

static int workspace_index_unchanged_at_0(struct test_ctx *ctx)
{
    return production_workspace_index(ctx) == 0;
}

static int trigger_show_desktop(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_SHOW_DESKTOP);
}

static int show_desktop_state_is_show(struct test_ctx *ctx)
{
    return production_show_desktop_state(ctx) == PROD_DESKTOP_SHOW;
}

static int show_desktop_state_is_normal(struct test_ctx *ctx)
{
    return production_show_desktop_state(ctx) == PROD_DESKTOP_NORMAL;
}

/* Unsupported (zoom) and unknown actions must be ignored, never fatal: the
 * connection stays usable afterwards. */
static int trigger_unsupported(struct test_ctx *ctx)
{
    return trigger(ctx, TREELAND_COMPOSITOR_ACTION_V1_ACTION_ZOOM_IN);
}

static int trigger_unknown(struct test_ctx *ctx)
{
    return trigger(ctx, 999);
}

static int connection_alive(struct test_ctx *ctx)
{
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int destroy_action(struct test_ctx *ctx)
{
    if (!ctx->action)
        return 0;
    treeland_compositor_action_v1_destroy(ctx->action);
    ctx->action = NULL;
    return 1;
}

static const struct test_case cases[] = {
    { "action.bind", bind_action },
    { "action.trigger.workspace_2", trigger_workspace_2 },
    { "action.workspace.switched_to_2", workspace_index_is_1 },
    { "action.trigger.prev_workspace", trigger_prev_workspace },
    { "action.workspace.switched_to_1", workspace_index_is_0 },
    { "action.trigger.next_workspace", trigger_next_workspace },
    { "action.workspace.switched_to_2_again", workspace_index_is_1 },
    { "action.trigger.workspace_1", trigger_workspace_1 },
    { "action.workspace.switched_to_1_again", workspace_index_is_0 },
    { "action.trigger.workspace_12_out_of_range", trigger_workspace_12_out_of_range },
    { "action.workspace.out_of_range_ignored", workspace_index_unchanged_at_0 },
    { "action.trigger.show_desktop", trigger_show_desktop },
    { "action.show_desktop.shown", show_desktop_state_is_show },
    { "action.trigger.show_desktop.again", trigger_show_desktop },
    { "action.show_desktop.restored", show_desktop_state_is_normal },
    { "action.trigger.unsupported.zoom", trigger_unsupported },
    { "action.trigger.unknown_value", trigger_unknown },
    { "action.connection.alive_after_ignored", connection_alive },
    { "action.destroy", destroy_action },
    { "action.connection.alive_after_destroy", connection_alive },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->action) treeland_compositor_action_v1_destroy(ctx->action);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_compositor_action_v1\n");
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
