// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-region-watch-unstable-v1-client-protocol.h"
#include "treeland-region-watch-unstable-v1.h"
#include "xdg-toplevel-client.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern void region_watch_query_state(void *data);
extern void region_watch_recheck(void *data);
extern void region_watch_output_position(void *data);

// Outputs are discovered dynamically: the test environment may contain
// additional headless outputs created by the production startup path, so
// sizes and creation order of the fixture outputs must not be hardcoded.
struct output_info {
    struct wl_output *proxy;
    char name[64];
    int mode_w;
    int mode_h;
    int have_name;
    int have_mode;
};

static struct output_info s_outputs[16];
static int s_output_count;

static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                            int32_t pw, int32_t ph, int32_t subpixel,
                            const char *make, const char *model, int32_t transform)
{
    (void)data; (void)output; (void)x; (void)y; (void)pw; (void)ph;
    (void)subpixel; (void)make; (void)model; (void)transform;
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh)
{
    (void)output; (void)refresh;
    if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
        return;
    struct output_info *info = data;
    info->mode_w = width;
    info->mode_h = height;
    info->have_mode = 1;
}

static void output_done(void *data, struct wl_output *output)
{
    (void)data; (void)output;
}

static void output_scale(void *data, struct wl_output *output, int32_t scale)
{
    (void)data; (void)output; (void)scale;
}

static void output_name(void *data, struct wl_output *output, const char *name)
{
    (void)output;
    struct output_info *info = data;
    snprintf(info->name, sizeof(info->name), "%s", name);
    info->have_name = 1;
}

static void output_description(void *data, struct wl_output *output, const char *description)
{
    (void)data; (void)output; (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static struct output_info *find_primary_output(void)
{
    struct output_info *best = NULL;
    for (int i = 0; i < s_output_count; ++i) {
        if (!s_outputs[i].have_name || !s_outputs[i].have_mode)
            continue;
        if (!best || strcmp(s_outputs[i].name, best->name) < 0)
            best = &s_outputs[i];
    }
    return best;
}

// Layout position of a bound output, queried from the server (wl_output
// geometry events always report (0,0)).
static int query_output_position(const struct output_info *info, int *x, int *y)
{
    struct region_watch_output_pos query;
    memset(&query, 0, sizeof(query));
    snprintf(query.name, sizeof(query.name), "%s", info->name);
    if (!invoke_on_server_thread(region_watch_output_position, &query) || !query.found)
        return 0;
    *x = query.x;
    *y = query.y;
    return 1;
}

struct test_ctx {
    struct client_connection connection;
    struct wl_display *display;
    struct treeland_region_watch_manager_v1 *manager;
    struct treeland_region_watch_v1 *watch;
    int enter_count;
    int leave_count;
    int output_removed_count;
};

static void watch_enter(void *data, struct treeland_region_watch_v1 *watch)
{
    (void)watch;
    ((struct test_ctx *)data)->enter_count++;
}

static void watch_leave(void *data, struct treeland_region_watch_v1 *watch)
{
    (void)watch;
    ((struct test_ctx *)data)->leave_count++;
}

static void watch_output_removed(void *data, struct treeland_region_watch_v1 *watch)
{
    (void)watch;
    ((struct test_ctx *)data)->output_removed_count++;
}

static const struct treeland_region_watch_v1_listener watch_listener = {
    .enter = watch_enter,
    .leave = watch_leave,
    .output_removed = watch_output_removed,
};

static int read_server_state(struct test_ctx *ctx, struct region_watch_server_state *state)
{
    if (wl_display_roundtrip(ctx->display) < 0)
        return 0;
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(region_watch_query_state, state);
}

// Synchronization boundary for asynchronous production actions: runs the
// exact overlap evaluation the production debounce timer runs, synchronously
// on the compositor thread, then dispatches the resulting events. No fixed
// delays or retry polling (framework sync rules).
static int recheck(struct test_ctx *ctx)
{
    if (invoke_on_server_thread(region_watch_recheck, NULL) <= 0)
        return 0;
    return wl_display_roundtrip(ctx->display) >= 0;
}

static int check_state(const struct region_watch_server_state *state,
                       int has_region,
                       int x,
                       int y,
                       int width,
                       int height)
{
    return state->has_watch && state->has_region == has_region && state->x == x
           && state->y == y && state->width == width && state->height == height;
}

int protocol_test_run(const char *socket_name)
{
    struct test_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    struct region_watch_server_state state;
    struct xdg_toplevel_client toplevel;
    struct output_info *primary = NULL;
    int px = 0, py = 0;

    if (!client_connect(&ctx.connection, socket_name)) {
        fprintf(stderr, "region-watch: failed to connect to the test server\n");
        goto failed;
    }
    ctx.display = ctx.connection.display;
    ctx.manager = client_bind(&ctx.connection, "treeland_region_watch_manager_v1",
                              &treeland_region_watch_manager_v1_interface, 1);

    // Bind every wl_output and discover names/modes.
    for (uint32_t i = 0; i < ctx.connection.global_count && s_output_count < 16; ++i) {
        const struct client_global *global = &ctx.connection.globals[i];
        if (strcmp(global->interface, "wl_output") != 0)
            continue;
        struct output_info *info = &s_outputs[s_output_count++];
        memset(info, 0, sizeof(*info));
        uint32_t version = global->version < 4 ? global->version : 4;
        info->proxy = wl_registry_bind(ctx.connection.registry, global->name,
                                       &wl_output_interface, version);
        if (info->proxy)
            wl_output_add_listener(info->proxy, &output_listener, info);
    }
    if (!ctx.manager || wl_display_roundtrip(ctx.display) < 0) {
        fprintf(stderr, "region-watch: failed to bind required globals\n");
        goto failed;
    }

    primary = find_primary_output();
    if (!primary || !primary->proxy) {
        fprintf(stderr, "region-watch: no usable primary output found\n");
        goto failed;
    }

    if (!query_output_position(primary, &px, &py)) {
        fprintf(stderr, "region-watch: failed to query output layout positions\n");
        goto failed;
    }

    // manager.get_region_watch
    ctx.watch = treeland_region_watch_manager_v1_get_region_watch(ctx.manager);
    if (!ctx.watch) {
        fprintf(stderr, "region-watch: get_region_watch failed\n");
        goto failed;
    }
    treeland_region_watch_v1_add_listener(ctx.watch, &watch_listener, &ctx);
    if (wl_display_roundtrip(ctx.display) < 0)
        goto failed;

    // watch.set_region: full primary output coverage. The region must be
    // recorded in global layout coordinates (output position + strip).
    treeland_region_watch_v1_set_region(ctx.watch, primary->mode_w, primary->mode_h,
                                        TREELAND_REGION_WATCH_V1_ANCHOR_TOP,
                                        primary->proxy);
    if (!read_server_state(&ctx, &state)
        || !check_state(&state, 1, px, py, primary->mode_w, primary->mode_h)) {
        fprintf(stderr, "region-watch: set_region(top) state mismatch: rect=(%d,%d %dx%d), expected (%d,%d %dx%d)\n",
                state.x, state.y, state.width, state.height,
                px, py, primary->mode_w, primary->mode_h);
        goto failed;
    }

    // The protocol requires evaluating the overlap state after each
    // set_region: with no windows the watcher must report the clear state.
    if (ctx.leave_count != 1 || ctx.enter_count != 0) {
        fprintf(stderr, "region-watch: expected one leave after set_region, got %d/%d\n",
                ctx.enter_count, ctx.leave_count);
        goto failed;
    }

    // Map a toplevel: the region now overlaps a real window.
    if (!xdg_toplevel_client_create_with_solid_buffer(&ctx.connection, &toplevel,
                                                      800, 600, 0xff3366cc)) {
        fprintf(stderr, "region-watch: failed to map the xdg toplevel\n");
        goto failed;
    }
    if (!recheck(&ctx) || ctx.enter_count != 1 || ctx.leave_count != 1) {
        fprintf(stderr, "region-watch: enter not delivered after mapping a window (%d/%d)\n",
                ctx.enter_count, ctx.leave_count);
        xdg_toplevel_client_destroy(&toplevel);
        goto failed;
    }
    xdg_toplevel_client_destroy(&toplevel);
    // Roundtrip so the server has processed the destroy and removed the
    // wrapper from its rect list before the evaluation runs.
    if (wl_display_roundtrip(ctx.display) < 0)
        goto failed;
    if (!recheck(&ctx) || ctx.leave_count != 2) {
        fprintf(stderr, "region-watch: leave not delivered after closing the window (%d)\n",
                ctx.leave_count);
        goto failed;
    }

    // Shrink the region to a thin strip; set_region must be evaluated again
    // and re-send the (still clear) state.
    treeland_region_watch_v1_set_region(ctx.watch, 100, 40,
                                        TREELAND_REGION_WATCH_V1_ANCHOR_TOP,
                                        primary->proxy);
    if (!read_server_state(&ctx, &state)
        || !check_state(&state, 1, px, py, primary->mode_w, 40)) {
        fprintf(stderr, "region-watch: set_region strip mismatch: rect=(%d,%d %dx%d), expected (%d,%d %dx40)\n",
                state.x, state.y, state.width, state.height, px, py, primary->mode_w);
        goto failed;
    }
    if (ctx.leave_count != 3) {
        fprintf(stderr, "region-watch: expected re-sent leave after strip set_region, got %d\n",
                ctx.leave_count);
        goto failed;
    }

    // TODO(multi-output/output_removed): exercise a second output, coordinate
    // translation, output_removed and the inert-until-set_region semantics
    // once the fixture wires a second headless output into the output layout
    // (so it has a WOutput wrapper and a layout position).

    // manager.destroy must not affect watchers: the watcher is still usable.
    treeland_region_watch_manager_v1_destroy(ctx.manager);
    ctx.manager = NULL;
    treeland_region_watch_v1_set_region(ctx.watch, primary->mode_w, primary->mode_h,
                                        TREELAND_REGION_WATCH_V1_ANCHOR_TOP,
                                        primary->proxy);
    if (!read_server_state(&ctx, &state)
        || !check_state(&state, 1, px, py, primary->mode_w, primary->mode_h)) {
        fprintf(stderr, "region-watch: watcher unusable after manager destroy\n");
        goto failed;
    }
    if (ctx.leave_count != 4) {
        fprintf(stderr, "region-watch: expected leave after post-removal set_region, got %d\n",
                ctx.leave_count);
        goto failed;
    }

    treeland_region_watch_v1_destroy(ctx.watch);
    ctx.watch = NULL;
    if (wl_display_roundtrip(ctx.display) < 0)
        goto failed;

    client_disconnect(&ctx.connection);
    printf("region-watch: all checks passed\n");
    return 0;

failed:
    client_disconnect(&ctx.connection);
    return 1;
}
