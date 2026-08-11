/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Pure-C Wayland client for the treeland-foreign-toplevel-manager-v1 protocol test.
 *
 * The manager never lets a client create a treeland_foreign_toplevel_handle_v1
 * by request: handles are pushed through the toplevel event, and only for
 * surfaces the compositor has wrapped server-side (SurfaceWrapper, which needs
 * a QmlEngine + WToplevelSurface), so the handle requests and the toplevel
 * event are out of reach here. What is honestly observable from a bare client:
 *   - the client creates an xdg_toplevel so its surface carries a waylib
 *     WSurface wrapper, which the dock preview context resolves;
 *   - get_dock_preview_context(surface) and its show / show_tooltip / close
 *     requests, whose server-side effect is the manager's requestDockPreview /
 *     requestDockPreviewTooltip / requestDockClose signals (captured by
 *     setup.cpp and read back via protocol_test_invoke_server);
 *   - the dock_preview_context enter/leave events, emitted by the server when
 *     the compositor drives enterDockPreview/leaveDockPreview (deliberate
 *     server-side stimuli, no client request exists for them);
 *   - the manager stop request, answered by the finished event.
 */
#include "treeland-foreign-toplevel-manager-v1.h"
#include "treeland-foreign-toplevel-manager-v1-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ftm_read_server_state(void *data);
extern void ftm_enter_dock_preview(void *data);
extern void ftm_leave_dock_preview(void *data);

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

static void manager_toplevel(void *data, struct treeland_foreign_toplevel_manager_v1 *manager,
                             struct treeland_foreign_toplevel_handle_v1 *toplevel)
{
    /* A bare client surface can never become a server-side toplevel, so this
     * event is not produced by any case; registered so a future compositor
     * change cannot silently drop the event. */
    (void)data;
    (void)manager;
    (void)toplevel;
}

static void manager_finished(void *data, struct treeland_foreign_toplevel_manager_v1 *manager)
{
    (void)manager;
    ((struct test_ctx *)data)->manager_finished_received = 1;
}

static const struct treeland_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = manager_toplevel,
    .finished = manager_finished,
};

static void context_enter(void *data, struct treeland_dock_preview_context_v1 *context)
{
    (void)context;
    ((struct test_ctx *)data)->context_enter_received = 1;
}

static void context_leave(void *data, struct treeland_dock_preview_context_v1 *context)
{
    (void)context;
    ((struct test_ctx *)data)->context_leave_received = 1;
}

static const struct treeland_dock_preview_context_v1_listener context_listener = {
    .enter = context_enter,
    .leave = context_leave,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!protocol_test_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->manager = protocol_test_bind(&ctx->connection, "treeland_foreign_toplevel_manager_v1",
                                      &treeland_foreign_toplevel_manager_v1_interface, 1);
    if (ctx->manager)
        treeland_foreign_toplevel_manager_v1_add_listener(ctx->manager, &manager_listener, ctx);
    return ctx->manager != NULL;
}

static int read_server_state(struct test_ctx *ctx, struct ftm_server_state *state)
{
    (void)ctx;
    memset(state, 0, sizeof(*state));
    return protocol_test_invoke_server(ftm_read_server_state, state);
}

static int create_xdg_toplevel(struct test_ctx *ctx)
{
    if (!protocol_test_xdg_toplevel_create(&ctx->connection, &ctx->xdg_toplevel))
        return 0;
    struct ftm_server_state state;
    return ctx->xdg_toplevel.configured && read_server_state(ctx, &state)
           && state.mapped_xdg_toplevel;
}

static int create_context(struct test_ctx *ctx)
{
    if (!ctx->xdg_toplevel.surface)
        return 0;
    ctx->context = treeland_foreign_toplevel_manager_v1_get_dock_preview_context(
        ctx->manager, ctx->xdg_toplevel.surface);
    if (ctx->context)
        treeland_dock_preview_context_v1_add_listener(ctx->context, &context_listener, ctx);
    return ctx->context != NULL;
}

static int show_preview(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    struct wl_array surfaces;
    wl_array_init(&surfaces);
    /* Empty identifier list: the request must still be accepted and the server
     * must emit requestDockPreview with no resolved toplevels. */
    treeland_dock_preview_context_v1_show(ctx->context, &surfaces, 10, 20,
                                          TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_BOTTOM);
    wl_array_release(&surfaces);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.preview_fired
           && state.preview_x == 10
           && state.preview_y == 20
           && state.preview_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_BOTTOM
           && state.preview_surface_count == 0;
}

static int show_unknown_identifier(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    struct wl_array surfaces;
    wl_array_init(&surfaces);
    uint32_t *slot = wl_array_add(&surfaces, sizeof(uint32_t));
    if (!slot)
        return 0;
    *slot = 0xDEADBEEFu;
    /* No handle can exist for this identifier (no toplevels at all); the server
     * must silently ignore it and accept the request without a protocol error. */
    treeland_dock_preview_context_v1_show(ctx->context, &surfaces, 1, 2,
                                          TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_TOP);
    wl_array_release(&surfaces);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.preview_fired && state.preview_surface_count == 0;
}

static int show_tooltip(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    treeland_dock_preview_context_v1_show_tooltip(ctx->context, "dock-tooltip", 5, 6,
                                                  TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_TOP);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.tooltip_fired
           && strcmp(state.tooltip, "dock-tooltip") == 0
           && state.tooltip_x == 5
           && state.tooltip_y == 6
           && state.tooltip_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_TOP;
}

static int close_preview(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    treeland_dock_preview_context_v1_close(ctx->context);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.close_fired;
}

static int enter_dock_preview(struct test_ctx *ctx)
{
    if (!protocol_test_invoke_server(ftm_enter_dock_preview, NULL))
        return 0;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->context_enter_received;
}

static int leave_dock_preview(struct test_ctx *ctx)
{
    if (!protocol_test_invoke_server(ftm_leave_dock_preview, NULL))
        return 0;
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->context_leave_received;
}

static int destroy_context(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    treeland_dock_preview_context_v1_destroy(ctx->context);
    ctx->context = NULL;
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int stop_manager(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    treeland_foreign_toplevel_manager_v1_stop(ctx->manager);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->manager_finished_received;
}

static const struct test_case cases[] = {
    { "client.create_xdg_toplevel", create_xdg_toplevel },
    { "manager.get_dock_preview_context", create_context },
    { "context.show", show_preview },
    { "context.show_unknown_identifier", show_unknown_identifier },
    { "context.show_tooltip", show_tooltip },
    { "context.close", close_preview },
    { "server.enter_dock_preview", enter_dock_preview },
    { "server.leave_dock_preview", leave_dock_preview },
    { "context.destroy", destroy_context },
    { "manager.stop", stop_manager },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->context) treeland_dock_preview_context_v1_destroy(ctx->context);
    protocol_test_xdg_toplevel_destroy(&ctx->xdg_toplevel);
    if (ctx->manager) treeland_foreign_toplevel_manager_v1_destroy(ctx->manager);
    protocol_test_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_foreign_toplevel_manager_v1\n");
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
