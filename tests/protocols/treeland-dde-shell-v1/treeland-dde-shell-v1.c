// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-dde-shell-v1.h"
#include "server-bridge-api.h"
#include "treeland-dde-shell-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void dde_shell_emit_test_events(void *data);

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

static void checker_enter(void *data, struct treeland_window_overlap_checker *checker)
{
    (void)checker;
    ((struct test_ctx *)data)->checker_enter_received = 1;
}

static void checker_leave(void *data, struct treeland_window_overlap_checker *checker)
{
    (void)checker;
    ((struct test_ctx *)data)->checker_leave_received = 1;
}

static const struct treeland_window_overlap_checker_listener checker_listener = {
    .enter = checker_enter,
    .leave = checker_leave,
};

static void active_in(void *data, struct treeland_dde_active_v1 *active, uint32_t reason)
{
    (void)active;
    ((struct test_ctx *)data)->active_in_received = (int)reason;
}

static void active_out(void *data, struct treeland_dde_active_v1 *active, uint32_t reason)
{
    (void)active;
    ((struct test_ctx *)data)->active_out_received = (int)reason;
}

static void start_drag(void *data, struct treeland_dde_active_v1 *active)
{
    (void)active;
    ((struct test_ctx *)data)->start_drag_received = 1;
}

static void drop(void *data, struct treeland_dde_active_v1 *active)
{
    (void)active;
    ((struct test_ctx *)data)->drop_received = 1;
}

static const struct treeland_dde_active_v1_listener active_listener = {
    .active_in = active_in,
    .active_out = active_out,
    .start_drag = start_drag,
    .drop = drop,
};

static void picker_window(void *data, struct treeland_window_picker_v1 *picker, int32_t pid)
{
    (void)picker;
    struct test_ctx *ctx = data;
    ctx->picker_window_received = 1;
    ctx->picker_pid = pid;
}

static const struct treeland_window_picker_v1_listener picker_listener = {
    .window = picker_window,
};

extern void dde_shell_query_surface_state(void *data);

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor", &wl_compositor_interface, 1);
    ctx->seat = client_bind(&ctx->connection, "wl_seat", &wl_seat_interface, 1);
    ctx->output = client_bind(&ctx->connection, "wl_output", &wl_output_interface, 1);
    ctx->manager = client_bind(&ctx->connection, "treeland_dde_shell_manager_v1",
                                      &treeland_dde_shell_manager_v1_interface, 1);
    return ctx->manager != NULL;
}

static int create_checker(struct test_ctx *ctx)
{
    ctx->checker = treeland_dde_shell_manager_v1_get_window_overlap_checker(ctx->manager);
    if (ctx->checker)
        treeland_window_overlap_checker_add_listener(ctx->checker, &checker_listener, ctx);
    return ctx->checker != NULL;
}

static int create_shell_surface(struct test_ctx *ctx)
{
    if (!ctx->compositor)
        return 0;
    ctx->test_surface = wl_compositor_create_surface(ctx->compositor);
    ctx->shell_surface = treeland_dde_shell_manager_v1_get_shell_surface(ctx->manager, ctx->test_surface);
    return ctx->test_surface && ctx->shell_surface;
}

static int create_active(struct test_ctx *ctx)
{
    if (!ctx->seat)
        return 0;
    ctx->active = treeland_dde_shell_manager_v1_get_treeland_dde_active(ctx->manager, ctx->seat);
    if (ctx->active)
        treeland_dde_active_v1_add_listener(ctx->active, &active_listener, ctx);
    return ctx->active != NULL;
}

static int create_multitaskview(struct test_ctx *ctx)
{
    ctx->multitaskview = treeland_dde_shell_manager_v1_get_treeland_multitaskview(ctx->manager);
    return ctx->multitaskview != NULL;
}

static int create_picker(struct test_ctx *ctx)
{
    ctx->picker = treeland_dde_shell_manager_v1_get_treeland_window_picker(ctx->manager);
    if (ctx->picker)
        treeland_window_picker_v1_add_listener(ctx->picker, &picker_listener, ctx);
    return ctx->picker != NULL;
}

static int create_lockscreen(struct test_ctx *ctx)
{
    ctx->lockscreen = treeland_dde_shell_manager_v1_get_treeland_lockscreen(ctx->manager);
    return ctx->lockscreen != NULL;
}

static int update_checker(struct test_ctx *ctx)
{
    if (!ctx->output)
        return 0;
    treeland_window_overlap_checker_update(ctx->checker, 100, 100,
                                           TREELAND_WINDOW_OVERLAP_CHECKER_ANCHOR_TOP, ctx->output);
    return 1;
}

static int read_shell_surface_state(struct test_ctx *ctx, struct dde_shell_surface_state *state)
{
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(dde_shell_query_surface_state, state);
}

static int set_surface_position(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_surface_position(ctx->shell_surface, 42, 24);
    return read_shell_surface_state(ctx, &state)
           && state.position_x == 42 && state.position_y == 24;
}

static int set_surface_role(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_role(ctx->shell_surface,
                                            TREELAND_DDE_SHELL_SURFACE_V1_ROLE_OVERLAY);
    return read_shell_surface_state(ctx, &state) && state.role_overlay;
}

static int set_auto_placement(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_auto_placement(ctx->shell_surface, 37);
    return read_shell_surface_state(ctx, &state) && state.auto_placement == 37;
}

static int set_skip_switcher(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_skip_switcher(ctx->shell_surface, 1);
    return read_shell_surface_state(ctx, &state) && state.skip_switcher;
}

static int set_skip_dock_preview(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_skip_dock_preview(ctx->shell_surface, 1);
    return read_shell_surface_state(ctx, &state) && state.skip_dock_preview;
}

static int set_skip_multitask_view(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_skip_muti_task_view(ctx->shell_surface, 1);
    return read_shell_surface_state(ctx, &state) && state.skip_multitask_view;
}

static int set_keyboard_focus(struct test_ctx *ctx)
{
    struct dde_shell_surface_state state;
    treeland_dde_shell_surface_v1_set_accept_keyboard_focus(ctx->shell_surface, 0);
    return read_shell_surface_state(ctx, &state) && !state.accept_keyboard_focus;
}
static int shell_surface_state(struct test_ctx *ctx)
{
    (void)ctx;
    struct dde_shell_surface_state state;
    return invoke_on_server_thread(dde_shell_query_surface_state, &state)
        && state.position_x == 42 && state.position_y == 24 && state.role_overlay
        && state.auto_placement == 37 && state.skip_switcher && state.skip_dock_preview
        && state.skip_multitask_view && !state.accept_keyboard_focus;
}
static int toggle_multitaskview(struct test_ctx *ctx)
{
    treeland_multitaskview_v1_toggle(ctx->multitaskview);
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int lock(struct test_ctx *ctx)
{
    treeland_lockscreen_v1_lock(ctx->lockscreen);
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int shutdown(struct test_ctx *ctx)
{
    treeland_lockscreen_v1_shutdown(ctx->lockscreen);
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int switch_user(struct test_ctx *ctx)
{
    treeland_lockscreen_v1_switch_user(ctx->lockscreen);
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int pick(struct test_ctx *ctx)
{
    treeland_window_picker_v1_pick(ctx->picker, "test-hint");
    return wl_display_roundtrip(ctx->display) >= 0;
}
static int emit_events(struct test_ctx *ctx) { (void)ctx; return invoke_on_server_thread(dde_shell_emit_test_events, NULL); }
static int checker_enter_received(struct test_ctx *ctx) { return ctx->checker_enter_received; }
static int checker_leave_received(struct test_ctx *ctx) { return ctx->checker_leave_received; }
static int active_in_received(struct test_ctx *ctx) { return ctx->active_in_received == 0; }
static int active_out_received(struct test_ctx *ctx) { return ctx->active_out_received == 1; }
static int start_drag_received(struct test_ctx *ctx) { return ctx->start_drag_received; }
static int drop_received(struct test_ctx *ctx) { return ctx->drop_received; }
static int picker_window_received(struct test_ctx *ctx) { return ctx->picker_window_received && ctx->picker_pid == 42; }

static const struct test_case cases[] = {
    { "manager.get_window_overlap_checker", create_checker },
    { "manager.get_shell_surface", create_shell_surface },
    { "manager.get_treeland_dde_active", create_active },
    { "manager.get_treeland_multitaskview", create_multitaskview },
    { "manager.get_treeland_window_picker", create_picker },
    { "manager.get_treeland_lockscreen", create_lockscreen },
    { "checker.update", update_checker },
    { "shell_surface.set_surface_position", set_surface_position },
    { "shell_surface.set_role", set_surface_role },
    { "shell_surface.set_auto_placement", set_auto_placement },
    { "shell_surface.set_skip_switcher", set_skip_switcher },
    { "shell_surface.set_skip_dock_preview", set_skip_dock_preview },
    { "shell_surface.set_skip_multitask_view", set_skip_multitask_view },
    { "shell_surface.set_accept_keyboard_focus", set_keyboard_focus },
    { "server.shell_surface_state", shell_surface_state },
    { "request.multitaskview.toggle_dispatch", toggle_multitaskview },
    { "request.lockscreen.lock_dispatch", lock },
    { "request.lockscreen.shutdown_dispatch", shutdown },
    { "request.lockscreen.switch_user_dispatch", switch_user },
    { "request.picker.pick_dispatch", pick },
    { "server.emit_events", emit_events },
    { "checker.event.enter", checker_enter_received },
    { "checker.event.leave", checker_leave_received },
    { "active.event.active_in", active_in_received },
    { "active.event.active_out", active_out_received },
    { "active.event.start_drag", start_drag_received },
    { "active.event.drop", drop_received },
    { "picker.event.window", picker_window_received },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->shell_surface) treeland_dde_shell_surface_v1_destroy(ctx->shell_surface);
    if (ctx->checker) treeland_window_overlap_checker_destroy(ctx->checker);
    if (ctx->multitaskview) treeland_multitaskview_v1_destroy(ctx->multitaskview);
    if (ctx->picker) treeland_window_picker_v1_destroy(ctx->picker);
    if (ctx->lockscreen) treeland_lockscreen_v1_destroy(ctx->lockscreen);
    if (ctx->active) treeland_dde_active_v1_destroy(ctx->active);
    if (ctx->manager) treeland_dde_shell_manager_v1_destroy(ctx->manager);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_dde_shell_manager_v1\n");
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
