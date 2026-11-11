// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treeland-foreign-toplevel-manager-v3.h"
#include "server-bridge-api.h"
#include "treeland-foreign-toplevel-manager-unstable-v3-client-protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

extern void ftm_read_server_state(void *data);
extern void ftm_render_and_settle(void *data);

struct test_case {
    const char *name;
    int (*run)(struct test_ctx *ctx);
};

// Layouts the compositor reports for a preview request: show always asks for
// the thumbnail layout, show_list for the list.
enum preview_layout_value {
    PREVIEW_LAYOUT_THUMBNAIL = 0,
    PREVIEW_LAYOUT_LIST = 1,
};

typedef void (*ftm_layout_request_fn)(struct treeland_foreign_toplevel_handle_v3 *handle);

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

static void handle_pid(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                       uint32_t pid)
{
    (void)data;
    (void)handle;
    (void)pid;
}

static void handle_title(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                         const char *title)
{
    (void)data;
    (void)handle;
    (void)title;
}

static void handle_app_id(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                          const char *app_id)
{
    (void)data;
    (void)handle;
    (void)app_id;
}

static void handle_identifier(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                              uint32_t identifier)
{
    (void)handle;
    ((struct test_ctx *)data)->handle_identifier = identifier;
}

static void handle_output_enter(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                                struct wl_output *output)
{
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_output_leave(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                                struct wl_output *output)
{
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_state(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                         struct wl_array *state)
{
    (void)data;
    (void)handle;
    (void)state;
}

static void handle_done(void *data, struct treeland_foreign_toplevel_handle_v3 *handle)
{
    (void)data;
    (void)handle;
}

static void handle_closed(void *data, struct treeland_foreign_toplevel_handle_v3 *handle)
{
    struct test_ctx *ctx = data;
    if (ctx->handle == handle)
        ctx->handle = NULL;
    ++ctx->handle_closed_count;
    treeland_foreign_toplevel_handle_v3_destroy(handle);
}

static void handle_parent(void *data, struct treeland_foreign_toplevel_handle_v3 *handle,
                          struct treeland_foreign_toplevel_handle_v3 *parent)
{
    (void)data;
    (void)handle;
    (void)parent;
}

static const struct treeland_foreign_toplevel_handle_v3_listener handle_listener = {
    .pid = handle_pid,
    .title = handle_title,
    .app_id = handle_app_id,
    .identifier = handle_identifier,
    .output_enter = handle_output_enter,
    .output_leave = handle_output_leave,
    .state = handle_state,
    .done = handle_done,
    .closed = handle_closed,
    .parent = handle_parent,
};

static void manager_toplevel(void *data, struct treeland_foreign_toplevel_manager_v3 *manager,
                             struct treeland_foreign_toplevel_handle_v3 *toplevel)
{
    (void)manager;
    struct test_ctx *ctx = data;
    ctx->handle = toplevel;
    ++ctx->handle_count;
    treeland_foreign_toplevel_handle_v3_add_listener(toplevel, &handle_listener, ctx);
}

static void manager_finished(void *data, struct treeland_foreign_toplevel_manager_v3 *manager)
{
    (void)manager;
    ((struct test_ctx *)data)->manager_finished_received = 1;
}

static const struct treeland_foreign_toplevel_manager_v3_listener manager_listener = {
    .toplevel = manager_toplevel,
    .finished = manager_finished,
};

static void context_enter(void *data, struct treeland_dock_preview_context_v3 *context)
{
    (void)context;
    ((struct test_ctx *)data)->context_enter_received = 1;
}

static void context_leave(void *data, struct treeland_dock_preview_context_v3 *context)
{
    (void)context;
    ((struct test_ctx *)data)->context_leave_received = 1;
}

static const struct treeland_dock_preview_context_v3_listener context_listener = {
    .enter = context_enter,
    .leave = context_leave,
};

static int connect_client(struct test_ctx *ctx, const char *socket_name)
{
    if (!client_connect(&ctx->connection, socket_name))
        return 0;
    ctx->display = ctx->connection.display;
    ctx->seat = client_bind(&ctx->connection, "wl_seat", &wl_seat_interface, 1);
    ctx->compositor = client_bind(&ctx->connection, "wl_compositor",
                                  &wl_compositor_interface, 1);
    ctx->shm = client_bind(&ctx->connection, "wl_shm", &wl_shm_interface, 1);
    // Version 2 is what carries show_list; the handle interface stays at
    // version 1 and the compositor must clamp it.
    ctx->manager = client_bind(&ctx->connection, "treeland_foreign_toplevel_manager_v3",
                                      &treeland_foreign_toplevel_manager_v3_interface, 2);
    if (ctx->manager)
        treeland_foreign_toplevel_manager_v3_add_listener(ctx->manager, &manager_listener, ctx);
    return ctx->seat != NULL && ctx->compositor != NULL && ctx->shm != NULL
           && ctx->manager != NULL;
}

static int read_server_state(struct test_ctx *ctx, struct ftm_server_state *state)
{
    (void)ctx;
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(ftm_read_server_state, state);
}

static int settle_geometry_animation(struct test_ctx *ctx)
{
    (void)ctx;
    return invoke_on_server_thread(ftm_render_and_settle, NULL);
}

static int create_xdg_toplevel(struct test_ctx *ctx)
{
    if (!xdg_toplevel_client_create(&ctx->connection, &ctx->xdg_toplevel))
        return 0;
    struct ftm_server_state state;
    return wl_display_roundtrip(ctx->display) >= 0
           && ctx->xdg_toplevel.configured
           && ctx->handle
           && ctx->handle_count == 1
           && ctx->handle_identifier
           && settle_geometry_animation(ctx)
           && read_server_state(ctx, &state)
           && state.wrapper_created
           && state.wrapper_in_workspace;
}

static int create_context(struct test_ctx *ctx)
{
    if (!ctx->xdg_toplevel.surface)
        return 0;
    ctx->context = treeland_foreign_toplevel_manager_v3_get_dock_preview_context(
        ctx->manager, ctx->xdg_toplevel.surface);
    if (ctx->context)
        treeland_dock_preview_context_v3_add_listener(ctx->context, &context_listener, ctx);
    return ctx->context != NULL;
}

static int show_preview(struct test_ctx *ctx)
{
    if (!ctx->context || !ctx->handle_identifier)
        return 0;
    struct wl_array surfaces;
    wl_array_init(&surfaces);
    uint32_t *slot = wl_array_add(&surfaces, sizeof(uint32_t));
    if (!slot) {
        wl_array_release(&surfaces);
        return 0;
    }
    *slot = ctx->handle_identifier;
    treeland_dock_preview_context_v3_show(ctx->context, &surfaces, 10, 20,
                                          TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_BOTTOM);
    wl_array_release(&surfaces);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.preview_fired
           && state.preview_x == 10
           && state.preview_y == 20
           && state.preview_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_BOTTOM
           && state.preview_layout == PREVIEW_LAYOUT_THUMBNAIL
           && state.preview_surface_count == 1;
}

static int show_list_preview(struct test_ctx *ctx)
{
    if (!ctx->context || !ctx->handle_identifier)
        return 0;
    struct wl_array surfaces;
    wl_array_init(&surfaces);
    uint32_t *slot = wl_array_add(&surfaces, sizeof(uint32_t));
    if (!slot) {
        wl_array_release(&surfaces);
        return 0;
    }
    *slot = ctx->handle_identifier;
    treeland_dock_preview_context_v3_show_list(
        ctx->context, &surfaces, 30, 40,
        TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_LEFT);
    wl_array_release(&surfaces);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.preview_fired
           && state.preview_x == 30
           && state.preview_y == 40
           && state.preview_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_LEFT
           && state.preview_layout == PREVIEW_LAYOUT_LIST
           && state.preview_surface_count == 1;
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

    treeland_dock_preview_context_v3_show(ctx->context, &surfaces, 1, 2,
                                          TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_TOP);
    wl_array_release(&surfaces);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.preview_fired && state.preview_surface_count == 0;
}

static int create_shm_surface_deferred(struct test_ctx *ctx, int width, int height,
                                       struct wl_surface **surface, struct wl_buffer **buffer)
{
    const int stride = width * 4;
    const size_t size = (size_t)stride * height;
    char name[64];
    snprintf(name, sizeof(name), "/treeland_ftm_tooltip_%d_%dx%d",
             (int)getpid(), width, height);
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return 0;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return 0;
    }
    // A translucent fill: the compositor's pill shows through where the client
    // paints nothing, which is what the tooltip contract asks for.
    uint32_t *pixels = data;
    for (size_t i = 0; i < size / sizeof(uint32_t); ++i)
        pixels[i] = 0x80000000u;

    struct wl_shm_pool *pool = wl_shm_create_pool(ctx->shm, fd, (int)size);
    close(fd);
    if (!pool) {
        munmap(data, size);
        return 0;
    }
    *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                        WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    if (!*buffer)
        return 0;

    *surface = wl_compositor_create_surface(ctx->compositor);
    if (!*surface)
        return 0;
    return 1;
}

static int commit_shm_surface(int width, int height,
                              struct wl_surface *surface, struct wl_buffer *buffer)
{
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
    return 1;
}

static int create_shm_surface(struct test_ctx *ctx, int width, int height,
                              struct wl_surface **surface, struct wl_buffer **buffer)
{
    if (!create_shm_surface_deferred(ctx, width, height, surface, buffer))
        return 0;
    return commit_shm_surface(width, height, *surface, *buffer);
}

static int show_tooltip_surface(struct test_ctx *ctx)
{
    struct ftm_server_state state;

    if (!ctx->context)
        return 0;
    // A surface without committed content is a client error: the request has no
    // effect and the compositor shows nothing for it.
    if (!create_shm_surface_deferred(ctx, 29, 19, &ctx->tooltip_surface, &ctx->tooltip_buffer))
        return 0;
    treeland_dock_preview_context_v3_show_tooltip_surface(
        ctx->context, ctx->tooltip_surface, 1, 2,
        TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_LEFT);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!read_server_state(ctx, &state))
        return 0;
    if (state.tooltip_count != 0)
        return 0;

    // The same surface is shown once its content has been committed.
    commit_shm_surface(29, 19, ctx->tooltip_surface, ctx->tooltip_buffer);
    treeland_dock_preview_context_v3_show_tooltip_surface(
        ctx->context, ctx->tooltip_surface, 5, 6,
        TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_TOP);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.tooltip_fired
           && state.tooltip_count == 1
           && state.tooltip_surface_present
           && state.tooltip_width == 29
           && state.tooltip_height == 19
           && state.tooltip_x == 5
           && state.tooltip_y == 6
           && state.tooltip_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_TOP;
}

static int replace_tooltip_surface(struct test_ctx *ctx)
{
    struct ftm_server_state state;

    if (!ctx->context)
        return 0;
    if (!create_shm_surface(ctx, 51, 17, &ctx->tooltip_surface2, &ctx->tooltip_buffer2))
        return 0;
    treeland_dock_preview_context_v3_show_tooltip_surface(
        ctx->context, ctx->tooltip_surface2, 7, 8,
        TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_BOTTOM);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.tooltip_count == 2
           && state.tooltip_surface_present
           && state.tooltip_width == 51
           && state.tooltip_height == 17
           && state.tooltip_x == 7
           && state.tooltip_y == 8
           && state.tooltip_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_BOTTOM;
}

static int move_tooltip_surface(struct test_ctx *ctx)
{
    struct ftm_server_state state;

    // Showing a surface that is already displayed only moves it: no commit is
    // needed for a new position.
    if (!ctx->context || !ctx->tooltip_surface2)
        return 0;
    treeland_dock_preview_context_v3_show_tooltip_surface(
        ctx->context, ctx->tooltip_surface2, 11, 12,
        TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_LEFT);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.tooltip_count == 3
           && state.tooltip_surface_present
           && state.tooltip_width == 51
           && state.tooltip_height == 17
           && state.tooltip_x == 11
           && state.tooltip_y == 12
           && state.tooltip_direction == TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_LEFT;
}

static int close_preview(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    treeland_dock_preview_context_v3_close(ctx->context);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    if (!read_server_state(ctx, &state))
        return 0;
    return state.close_fired;
}

static int minimize_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_set_minimized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return read_server_state(ctx, &state) && state.wrapper_minimized;
}

static int restore_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_unset_minimized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return read_server_state(ctx, &state) && !state.wrapper_minimized;
}

static int render_ack_and_read_server_state(struct test_ctx *ctx, struct ftm_server_state *state)
{
    return settle_geometry_animation(ctx)
           && xdg_toplevel_client_ack_latest_configure(&ctx->connection, &ctx->xdg_toplevel)
           && settle_geometry_animation(ctx)
           && read_server_state(ctx, state);
}

static int maximize_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_set_maximized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return render_ack_and_read_server_state(ctx, &state) && state.wrapper_maximized;
}

static int unmaximize_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_unset_maximized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return render_ack_and_read_server_state(ctx, &state) && !state.wrapper_maximized;
}

static int fullscreen_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_set_fullscreen(ctx->handle, NULL);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return render_ack_and_read_server_state(ctx, &state) && state.wrapper_fullscreen;
}

static int unfullscreen_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_unset_fullscreen(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return render_ack_and_read_server_state(ctx, &state) && !state.wrapper_fullscreen;
}

static void request_maximized(struct treeland_foreign_toplevel_handle_v3 *handle)
{
    treeland_foreign_toplevel_handle_v3_set_maximized(handle);
}

static void request_unmaximized(struct treeland_foreign_toplevel_handle_v3 *handle)
{
    treeland_foreign_toplevel_handle_v3_unset_maximized(handle);
}

static void request_fullscreen(struct treeland_foreign_toplevel_handle_v3 *handle)
{
    treeland_foreign_toplevel_handle_v3_set_fullscreen(handle, NULL);
}

static void request_unfullscreen(struct treeland_foreign_toplevel_handle_v3 *handle)
{
    treeland_foreign_toplevel_handle_v3_unset_fullscreen(handle);
}

static int minimize_keeps_layout_state(struct test_ctx *ctx,
                                       ftm_layout_request_fn request_layout,
                                       ftm_layout_request_fn request_unlayout,
                                       int layout_state)
{
    if (!ctx->handle)
        return 0;

    request_layout(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;

    struct ftm_server_state state;
    if (!render_ack_and_read_server_state(ctx, &state)
        || state.wrapper_state != layout_state)
        return 0;

    treeland_foreign_toplevel_handle_v3_set_minimized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!read_server_state(ctx, &state))
        return 0;
    if (!state.wrapper_self_minimized || state.wrapper_state != layout_state
        || state.wrapper_visible)
        return 0;

    treeland_foreign_toplevel_handle_v3_unset_minimized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!render_ack_and_read_server_state(ctx, &state))
        return 0;
    if (state.wrapper_self_minimized || state.wrapper_state != layout_state
        || !state.wrapper_visible)
        return 0;

    request_unlayout(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!render_ack_and_read_server_state(ctx, &state))
        return 0;
    return !state.wrapper_self_minimized && state.wrapper_visible
           && state.wrapper_state == FTM_WRAPPER_STATE_NORMAL;
}

static int minimize_keeps_maximized_state(struct test_ctx *ctx)
{
    return minimize_keeps_layout_state(ctx,
                                       request_maximized,
                                       request_unmaximized,
                                       FTM_WRAPPER_STATE_MAXIMIZED);
}

static int minimize_keeps_fullscreen_state(struct test_ctx *ctx)
{
    return minimize_keeps_layout_state(ctx,
                                       request_fullscreen,
                                       request_unfullscreen,
                                       FTM_WRAPPER_STATE_FULLSCREEN);
}

static int activate_real_toplevel(struct test_ctx *ctx)
{
    if (!ctx->handle || !ctx->seat)
        return 0;
    treeland_foreign_toplevel_handle_v3_activate(ctx->handle, ctx->seat);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return read_server_state(ctx, &state) && state.wrapper_activated && state.wrapper_focused;
}

static int set_icon_geometry_changes_icon_geometry(struct test_ctx *ctx)
{
    if (!ctx->handle || !ctx->xdg_toplevel.surface)
        return 0;
    treeland_foreign_toplevel_handle_v3_set_icon_geometry(ctx->handle,
                                                       ctx->xdg_toplevel.surface,
                                                       11, 12, 130, 140);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    struct ftm_server_state state;
    return read_server_state(ctx, &state)
           && state.icon_x == state.wrapper_x + 11 && state.icon_y == state.wrapper_y + 12
           && state.icon_width == 130 && state.icon_height == 140;
}

static int maximize_restore_in_flight(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    struct ftm_server_state state;
    // Start from a settled Normal state.
    treeland_foreign_toplevel_handle_v3_unset_fullscreen(ctx->handle);
    treeland_foreign_toplevel_handle_v3_unset_maximized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!render_ack_and_read_server_state(ctx, &state)
        || state.wrapper_state != FTM_WRAPPER_STATE_NORMAL)
        return 0;

    // Maximize, then restore without letting the transition settle. Queue both
    // requests before a single roundtrip so the server processes the retarget
    // before any render frame or animation timer can settle the first one.
    treeland_foreign_toplevel_handle_v3_set_maximized(ctx->handle);
    treeland_foreign_toplevel_handle_v3_unset_maximized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;

    return render_ack_and_read_server_state(ctx, &state)
           && state.wrapper_state == FTM_WRAPPER_STATE_NORMAL;
}

static int maximize_during_fullscreen_cancel(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    struct ftm_server_state state;

    // Settle on Maximized so the effective state is Maximized.
    treeland_foreign_toplevel_handle_v3_set_maximized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    if (!render_ack_and_read_server_state(ctx, &state)
        || state.wrapper_state != FTM_WRAPPER_STATE_MAXIMIZED)
        return 0;

    // Start a fullscreen transition, cancel it, then request the already
    // effective Maximized state while the transition is still in flight. Queue
    // all three requests before a single roundtrip so the server handles the
    // retarget before any render frame or animation timer can settle it. This
    // used to build a degenerate animation (target == effective state) whose
    // first ready hit Q_ASSERT(m_pendingState != m_surfaceState) and aborted
    // the whole compositor.
    treeland_foreign_toplevel_handle_v3_set_fullscreen(ctx->handle, NULL);
    treeland_foreign_toplevel_handle_v3_unset_fullscreen(ctx->handle);
    treeland_foreign_toplevel_handle_v3_set_maximized(ctx->handle);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;

    return render_ack_and_read_server_state(ctx, &state)
           && state.wrapper_state == FTM_WRAPPER_STATE_MAXIMIZED;
}

static int request_close(struct test_ctx *ctx)
{
    if (!ctx->handle)
        return 0;
    treeland_foreign_toplevel_handle_v3_close(ctx->handle);
    return wl_display_roundtrip(ctx->display) >= 0 && ctx->xdg_toplevel.close_received;
}

static int destroy_context(struct test_ctx *ctx)
{
    if (!ctx->context)
        return 0;
    treeland_dock_preview_context_v3_destroy(ctx->context);
    ctx->context = NULL;
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int stop_manager(struct test_ctx *ctx)
{
    if (!ctx->manager)
        return 0;
    treeland_foreign_toplevel_manager_v3_stop(ctx->manager);
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    return ctx->manager_finished_received;
}

static const struct test_case cases[] = {
    { "client.create_xdg_toplevel", create_xdg_toplevel },
    { "manager.get_dock_preview_context", create_context },
    { "context.show", show_preview },
    { "context.show_list", show_list_preview },
    { "context.show_unknown_identifier", show_unknown_identifier },
    { "context.show_tooltip_surface", show_tooltip_surface },
    { "context.show_tooltip_surface_replace", replace_tooltip_surface },
    { "context.show_tooltip_surface_move", move_tooltip_surface },
    { "context.close", close_preview },
    { "handle.minimize_changes_wrapper", minimize_real_toplevel },
    { "handle.restore_changes_wrapper", restore_real_toplevel },
    { "handle.maximize_changes_wrapper", maximize_real_toplevel },
    { "handle.maximize_restore_in_flight", maximize_restore_in_flight },
    { "handle.maximize_during_fullscreen_cancel", maximize_during_fullscreen_cancel },
    { "handle.unmaximize_changes_wrapper", unmaximize_real_toplevel },
    { "handle.fullscreen_changes_wrapper", fullscreen_real_toplevel },
    { "handle.unfullscreen_changes_wrapper", unfullscreen_real_toplevel },
    { "handle.minimize_keeps_maximized_state", minimize_keeps_maximized_state },
    { "handle.minimize_keeps_fullscreen_state", minimize_keeps_fullscreen_state },
    { "handle.activate_focuses_wrapper", activate_real_toplevel },
    { "handle.set_icon_geometry_changes_icon_geometry", set_icon_geometry_changes_icon_geometry },
    { "handle.close_requests_xdg_close", request_close },
    { "context.destroy", destroy_context },
    { "manager.stop", stop_manager },
};

void test_cleanup(struct test_ctx *ctx)
{
    if (ctx->context) treeland_dock_preview_context_v3_destroy(ctx->context);
    xdg_toplevel_client_destroy(&ctx->xdg_toplevel);
    if (ctx->tooltip_buffer2) wl_buffer_destroy(ctx->tooltip_buffer2);
    if (ctx->tooltip_surface2) wl_surface_destroy(ctx->tooltip_surface2);
    if (ctx->tooltip_buffer) wl_buffer_destroy(ctx->tooltip_buffer);
    if (ctx->tooltip_surface) wl_surface_destroy(ctx->tooltip_surface);
    if (ctx->handle) treeland_foreign_toplevel_handle_v3_destroy(ctx->handle);
    if (ctx->manager) treeland_foreign_toplevel_manager_v3_destroy(ctx->manager);
    if (ctx->shm) wl_shm_destroy(ctx->shm);
    if (ctx->compositor) wl_compositor_destroy(ctx->compositor);
    if (ctx->seat) wl_seat_destroy(ctx->seat);
    client_disconnect(&ctx->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    test_init(&ctx);
    if (!connect_client(&ctx, socket_name)) {
        fprintf(stderr, "failed to connect to or bind treeland_foreign_toplevel_manager_v3\n");
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
