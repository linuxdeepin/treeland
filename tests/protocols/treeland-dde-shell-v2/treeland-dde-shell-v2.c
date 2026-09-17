// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-dde-shell-v2.h"
#include "server-bridge-api.h"
#include "treeland-dde-shell-unstable-v2-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void dde_shell_v2_query_surface_state(void *data);

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
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    ctx->seat = client_bind(&ctx->connection, "wl_seat", &wl_seat_interface, 1);
    ctx->output = client_bind(&ctx->connection, "wl_output", &wl_output_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_dde_shell_manager_v2",
                                      &treeland_dde_shell_manager_v2_interface, 1);
    return ctx->manager != NULL;
}

static int create_shell_surface(struct test_ctx *ctx)
{
    if (!ctx->compositor)
        return 0;
    ctx->test_surface = wl_compositor_create_surface(ctx->compositor);
    ctx->shell_surface = treeland_dde_shell_manager_v2_get_shell_surface(ctx->manager,
                                                                          ctx->test_surface);
    return ctx->test_surface && ctx->shell_surface;
}

static int read_shell_surface_state(struct test_ctx *ctx, struct dde_shell_surface_v2_state *state)
{
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(dde_shell_v2_query_surface_state, state);
}

static int set_surface_role(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_role(ctx->shell_surface,
                                            TREELAND_DDE_SHELL_SURFACE_V2_ROLE_OVERLAY);
    return read_shell_surface_state(ctx, &state) && state.role_overlay;
}

static int set_position_hint(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, ctx->output, 42, 24);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 42 && state.position_y == 24
           && !state.cursor_set;
}

static int set_position_hint_null_output(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, NULL, 7, 9);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 7 && state.position_y == 9;
}

// (0,0) is a valid fixed position (output origin / global origin), it must
// register as a present hint and not be swallowed as "no request".
static int set_position_hint_zero(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, NULL, 0, 0);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 0 && state.position_y == 0
           && !state.cursor_set;
}

static int set_cursor_placement_hint(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_cursor_placement_hint(ctx->shell_surface, 11, 5);
    return read_shell_surface_state(ctx, &state)
           && state.cursor_set && state.cursor_x == 11 && state.cursor_y == 5
           && !state.position_set;
}

static int placement_mode_switches_back(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_position_hint(ctx->shell_surface, NULL, 3, 4);
    return read_shell_surface_state(ctx, &state)
           && state.position_set && state.position_x == 3 && state.position_y == 4
           && !state.cursor_set;
}

static int set_skip_flags(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    const uint32_t all_flags = TREELAND_DDE_SHELL_SURFACE_V2_SKIP_FLAG_SWITCHER
        | TREELAND_DDE_SHELL_SURFACE_V2_SKIP_FLAG_DOCK_PREVIEW
        | TREELAND_DDE_SHELL_SURFACE_V2_SKIP_FLAG_MULTITASK_VIEW;
    treeland_dde_shell_surface_v2_set_skip_flags(ctx->shell_surface, all_flags);
    return read_shell_surface_state(ctx, &state) && state.skip_flags == (int)all_flags;
}

static int clear_skip_flags(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_skip_flags(ctx->shell_surface, 0);
    return read_shell_surface_state(ctx, &state) && state.skip_flags == 0;
}

static int set_keyboard_focus(struct test_ctx *ctx)
{
    struct dde_shell_surface_v2_state state;
    treeland_dde_shell_surface_v2_set_accept_keyboard_focus(ctx->shell_surface, 0);
    return read_shell_surface_state(ctx, &state) && !state.accept_keyboard_focus;
}

static int shell_surface_state(struct test_ctx *ctx)
{
    (void)ctx;
    struct dde_shell_surface_v2_state state;
    return invoke_on_server_thread(dde_shell_v2_query_surface_state, &state)
           && state.role_overlay && !state.accept_keyboard_focus;
}

static int destroy_shell_surface(struct test_ctx *ctx)
{
    treeland_dde_shell_surface_v2_destroy(ctx->shell_surface);
    ctx->shell_surface = NULL;
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int recreate_after_destroy(struct test_ctx *ctx)
{
    ctx->shell_surface = treeland_dde_shell_manager_v2_get_shell_surface(ctx->manager,
                                                                          ctx->test_surface);
    return ctx->shell_surface != NULL && wl_display_roundtrip(ctx->display) >= 0;
}

static int duplicate_shell_surface_error(struct test_ctx *ctx)
{
    treeland_dde_shell_manager_v2_get_shell_surface(ctx->manager, ctx->test_surface);
    // The compositor raises already_shell_surface, which kills the connection:
    // the roundtrip must fail.
    return wl_display_roundtrip(ctx->display) < 0;
}

static const struct test_case cases[] = {
    { "manager.get_shell_surface", create_shell_surface },
    { "shell_surface.set_role", set_surface_role },
    { "shell_surface.set_position_hint", set_position_hint },
    { "shell_surface.set_position_hint.null_output", set_position_hint_null_output },
    { "shell_surface.set_position_hint.zero", set_position_hint_zero },
    { "shell_surface.set_cursor_placement_hint", set_cursor_placement_hint },
    { "shell_surface.placement_mode_switches_back", placement_mode_switches_back },
    { "shell_surface.set_skip_flags", set_skip_flags },
    { "shell_surface.clear_skip_flags", clear_skip_flags },
    { "shell_surface.set_accept_keyboard_focus", set_keyboard_focus },
    { "server.shell_surface_state", shell_surface_state },
    { "shell_surface.destroy", destroy_shell_surface },
    { "manager.recreate_after_destroy", recreate_after_destroy },
    // Must stay last: the expected protocol error tears the connection down.
    { "manager.get_shell_surface.duplicate_error", duplicate_shell_surface_error },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->shell_surface)
        treeland_dde_shell_surface_v2_destroy(ctx->shell_surface);
    if (ctx->manager)
        treeland_dde_shell_manager_v2_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_dde_shell_manager_v2\n");
        test_cleanup(&ctx);
        test_destroy(&ctx);
        return 1;
    }

    const size_t total_cases = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < total_cases; ++i) {
        const int result = test_add(&ctx, cases[i].name);
        if (!cases[i].run(&ctx))
            test_fail(&ctx, result, "assertion failed");
        // The final case expects the protocol error that kills the connection.
        if (i + 1 < total_cases && wl_display_roundtrip(ctx.display) < 0)
            test_fail(&ctx, result, "Wayland connection failed");
    }

    test_cleanup(&ctx);
    const int success = test_print_results(&ctx);
    test_destroy(&ctx);
    return success ? 0 : 1;
}
