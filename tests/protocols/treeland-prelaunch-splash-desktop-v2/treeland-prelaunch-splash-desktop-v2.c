// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-app-id-resolver-unstable-v2-client-protocol.h"
#include "treeland-foreign-toplevel-manager-unstable-v3-client-protocol.h"
#include "treeland-prelaunch-splash-desktop-v2.h"
#include "treeland-prelaunch-splash-unstable-v2-client-protocol.h"
#include "xdg-toplevel-client.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void prelaunch_splash_desktop_read_state(void *data);
extern void prelaunch_splash_desktop_wait_for_creation(void *data);
extern void prelaunch_splash_desktop_wait_for_destruction(void *data);
extern void prelaunch_splash_desktop_wait_converted(void *data);
extern void prelaunch_splash_desktop_probe_icon(void *data);

static const char *const splash_app_id = "org.deepin.treeland.protocol.splash";
static const char *const icon_app_id = "org.deepin.treeland.protocol.splash-icon";
static const char *const plain_app_id = "org.deepin.treeland.protocol.splash-icon.plain";

#define ICON_WIDTH 16
#define ICON_HEIGHT 16
#define ICON_ARGB 0xffff0000u /* opaque red, ARGB8888 */
/* The splash wrapper is 800x600; the real window must match to convert without
   a geometry animation. */
#define SPLASH_WINDOW_WIDTH 800
#define SPLASH_WINDOW_HEIGHT 600
#define PLAIN_WINDOW_WIDTH 320
#define PLAIN_WINDOW_HEIGHT 200
#define PREVIEW_SWITCHES 10
#define MAX_HANDLES 4

struct handle_entry {
    struct treeland_foreign_toplevel_handle_v3 *handle;
    char app_id[96];
    uint32_t identifier;
    int closed;
};

struct client_state {
    struct treeland_foreign_toplevel_manager_v3 *manager;
    struct treeland_dock_preview_context_v3 *context;
    struct handle_entry handles[MAX_HANDLES];
    int handle_count;
};

struct resolver_state {
    uint32_t request_id;
    int identify_count;
    int pidfd;
};

static void handle_pid(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, uint32_t pid)
{
    (void)data;
    (void)handle;
    (void)pid;
}

static void handle_app_id(void *data,
                          struct treeland_foreign_toplevel_handle_v3 *handle,
                          const char *app_id)
{
    (void)handle;
    struct handle_entry *entry = data;
    snprintf(entry->app_id, sizeof(entry->app_id), "%s", app_id ? app_id : "");
}

static void handle_identifier(void *data,
                              struct treeland_foreign_toplevel_handle_v3 *handle,
                              uint32_t identifier)
{
    (void)handle;
    struct handle_entry *entry = data;
    entry->identifier = identifier;
}

static void handle_title(void *data,
                         struct treeland_foreign_toplevel_handle_v3 *handle,
                         const char *title)
{
    (void)data;
    (void)handle;
    (void)title;
}

static void handle_output_enter(void *data,
                                struct treeland_foreign_toplevel_handle_v3 *handle,
                                struct wl_output *output)
{
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_output_leave(void *data,
                                struct treeland_foreign_toplevel_handle_v3 *handle,
                                struct wl_output *output)
{
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_state(void *data,
                         struct treeland_foreign_toplevel_handle_v3 *handle,
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
    struct handle_entry *entry = data;
    entry->closed = 1;
    entry->identifier = 0;
    entry->handle = NULL;
    treeland_foreign_toplevel_handle_v3_destroy(handle);
}

static void handle_parent(void *data,
                          struct treeland_foreign_toplevel_handle_v3 *handle,
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

static void manager_toplevel(void *data,
                             struct treeland_foreign_toplevel_manager_v3 *manager,
                             struct treeland_foreign_toplevel_handle_v3 *handle)
{
    (void)manager;
    struct client_state *state = data;
    if (state->handle_count >= MAX_HANDLES) {
        treeland_foreign_toplevel_handle_v3_destroy(handle);
        return;
    }
    struct handle_entry *entry = &state->handles[state->handle_count++];
    memset(entry, 0, sizeof(*entry));
    entry->handle = handle;
    treeland_foreign_toplevel_handle_v3_add_listener(handle, &handle_listener, entry);
}

static void manager_finished(void *data, struct treeland_foreign_toplevel_manager_v3 *manager)
{
    (void)data;
    (void)manager;
}

static const struct treeland_foreign_toplevel_manager_v3_listener manager_listener = {
    .toplevel = manager_toplevel,
    .finished = manager_finished,
};

static void context_enter(void *data, struct treeland_dock_preview_context_v3 *context)
{
    (void)data;
    (void)context;
}

static void context_leave(void *data, struct treeland_dock_preview_context_v3 *context)
{
    (void)data;
    (void)context;
}

static const struct treeland_dock_preview_context_v3_listener context_listener = {
    .enter = context_enter,
    .leave = context_leave,
};

static void identify_request(void *data,
                             struct treeland_app_id_resolver_v2 *resolver,
                             uint32_t request_id,
                             int32_t pidfd)
{
    (void)resolver;
    struct resolver_state *state = data;
    state->request_id = request_id;
    ++state->identify_count;
    state->pidfd = pidfd;
}

static const struct treeland_app_id_resolver_v2_listener resolver_listener = {
    .identify_request = identify_request,
};

static struct wl_buffer *create_solid_buffer(struct wl_shm *shm,
                                             int width,
                                             int height,
                                             uint32_t argb)
{
    const int stride = width * 4;
    const size_t size = (size_t)stride * height;
    char name[64];
    snprintf(name, sizeof(name), "/treeland_splash_desktop_%d_%dx%d", (int)getpid(), width, height);

    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return NULL;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return NULL;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    uint32_t *pixels = data;
    for (size_t i = 0; i < (size_t)width * height; ++i)
        pixels[i] = argb;

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                                         WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    return buffer;
}

static int state_has_production_splash(const struct prelaunch_splash_desktop_state *state)
{
    return state->output_ready && state->wrapper_created && state->wrapper_in_workspace
        && state->wrapper_is_splash && state->wrapper_has_qml_item && state->wrapper_width == 800
        && state->wrapper_height == 600 && strcmp(state->app_id, splash_app_id) == 0;
}

static int state_has_surviving_icon(const struct prelaunch_splash_desktop_state *state)
{
    return state->icon_ready && state->icon_width == ICON_WIDTH
        && state->icon_height == ICON_HEIGHT;
}

static uint32_t wait_identifier(struct client_connection *connection,
                                struct client_state *state,
                                const char *app_id)
{
    for (int roundtrip = 0; roundtrip < 50; ++roundtrip) {
        for (int i = 0; i < state->handle_count; ++i) {
            struct handle_entry *entry = &state->handles[i];
            if (!entry->closed && entry->identifier && strcmp(entry->app_id, app_id) == 0)
                return entry->identifier;
        }
        if (wl_display_roundtrip(connection->display) < 0)
            return 0;
    }
    return 0;
}

/* The plain window has no splash, so its app id is never resolved: identify it as
   the handle that is not the splash window's. */
static uint32_t wait_other_identifier(struct client_connection *connection,
                                      struct client_state *state,
                                      uint32_t known_identifier)
{
    for (int roundtrip = 0; roundtrip < 50; ++roundtrip) {
        for (int i = 0; i < state->handle_count; ++i) {
            struct handle_entry *entry = &state->handles[i];
            if (!entry->closed && entry->identifier && entry->identifier != known_identifier)
                return entry->identifier;
        }
        if (wl_display_roundtrip(connection->display) < 0)
            return 0;
    }
    return 0;
}

static int show_preview(struct client_connection *connection,
                        struct client_state *state,
                        uint32_t identifier)
{
    struct wl_array surfaces;
    wl_array_init(&surfaces);
    uint32_t *slot = wl_array_add(&surfaces, sizeof(uint32_t));
    if (!slot) {
        wl_array_release(&surfaces);
        return 0;
    }
    *slot = identifier;
    treeland_dock_preview_context_v3_show(state->context,
                                          &surfaces,
                                          420,
                                          1080,
                                          TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_BOTTOM);
    wl_array_release(&surfaces);
    return wl_display_roundtrip(connection->display) >= 0;
}

static int probe_icon(struct desktop_preview_icon_probe *probe, int expect_icon)
{
    memset(probe, 0, sizeof(*probe));
    probe->expect_icon = expect_icon;
    return invoke_on_server_thread(prelaunch_splash_desktop_probe_icon, probe);
}

static int icon_drawn(const struct desktop_preview_icon_probe *probe)
{
    return probe->preview_found && probe->icon_item_found && probe->icon_buffer_matches
        && probe->icon_visible && probe->icon_width > 0 && probe->icon_width == probe->icon_height
        && probe->icon_width == probe->title_row_height && probe->capture_ready
        && probe->pixel_red >= 250 && probe->pixel_green <= 5 && probe->pixel_blue <= 5
        && probe->pixel_alpha >= 250 && probe->buffer_warnings == 0;
}

static int icon_hidden(const struct desktop_preview_icon_probe *probe)
{
    return probe->preview_found && !probe->icon_item_found && probe->buffer_warnings == 0;
}

static void report_probe(const char *stage, const struct desktop_preview_icon_probe *probe)
{
    fprintf(stderr,
            "  %s: preview=%d item=%d matches=%d visible=%d size=%dx%d row=%d capture=%d "
            "rgba=(%d,%d,%d,%d) warnings=%d\n",
            stage,
            probe->preview_found,
            probe->icon_item_found,
            probe->icon_buffer_matches,
            probe->icon_visible,
            probe->icon_width,
            probe->icon_height,
            probe->title_row_height,
            probe->capture_ready,
            probe->pixel_red,
            probe->pixel_green,
            probe->pixel_blue,
            probe->pixel_alpha,
            probe->buffer_warnings);
}

#define MAP_STEP(step) \
    do { \
        if (!(step)) { \
            fprintf(stderr, "map_window failed: %s\n", #step); \
            wl_buffer_destroy(buffer); \
            return 0; \
        } \
    } while (0)

static int map_window(struct client_connection *connection,
                      struct resolver_state *resolver_state,
                      struct xdg_toplevel_client *toplevel,
                      struct treeland_app_id_resolver_v2 *resolver,
                      const char *app_id,
                      int width,
                      int height)
{
    struct wl_buffer *buffer = NULL;
    const int requests_before = resolver_state->identify_count;

    MAP_STEP(xdg_toplevel_client_create_pending(connection, toplevel));
    MAP_STEP(wl_display_roundtrip(connection->display) >= 0);

    if (resolver_state->identify_count > requests_before) {
        treeland_app_id_resolver_v2_respond(resolver,
                                            resolver_state->request_id,
                                            app_id,
                                            "org.deepin.Sandbox");
        if (resolver_state->pidfd >= 0) {
            close(resolver_state->pidfd);
            resolver_state->pidfd = -1;
        }
    }

    /* The size has to be attached before the configure: a same-size window
       converts straight through, a differing one would need the geometry
       animation, which the headless software renderer never schedules. */
    for (int roundtrip = 0; roundtrip < 50 && !toplevel->configured; ++roundtrip)
        MAP_STEP(wl_display_roundtrip(connection->display) >= 0);
    MAP_STEP(toplevel->configured);
    MAP_STEP(xdg_toplevel_client_ack_latest_configure(connection, toplevel));
    buffer = create_solid_buffer(toplevel->shm, width, height, 0xffffffffu);
    MAP_STEP(buffer != NULL);
    wl_surface_attach(toplevel->surface, buffer, 0, 0);
    wl_surface_damage(toplevel->surface, 0, 0, width, height);
    wl_surface_commit(toplevel->surface);
    MAP_STEP(wl_display_roundtrip(connection->display) >= 0);
    wl_buffer_destroy(buffer);
    return 1;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client splash_window = { 0 };
    struct xdg_toplevel_client plain_window = { 0 };
    struct client_state state = { 0 };
    struct resolver_state resolver_state = { .pidfd = -1 };
    struct prelaunch_splash_desktop_state desktop_state = { 0 };
    struct desktop_preview_icon_probe probe = { 0 };
    struct treeland_app_id_resolver_manager_v2 *resolver_manager = NULL;
    struct treeland_app_id_resolver_v2 *resolver = NULL;
    struct treeland_prelaunch_splash_manager_v2 *splash_manager = NULL;
    struct treeland_prelaunch_splash_v2 *splash = NULL;
    struct wl_shm *shm = NULL;
    struct wl_buffer *icon = NULL;
    uint32_t splash_identifier = 0;
    uint32_t plain_identifier = 0;
    int observed = 0;
    int result = 1;
    const char *stage = "connect";

    if (!client_connect(&connection, socket_name))
        goto done;

    stage = "bind globals";
    resolver_manager = client_bind(&connection,
                                   "treeland_app_id_resolver_manager_v2",
                                   &treeland_app_id_resolver_manager_v2_interface,
                                   1);
    splash_manager = client_bind(&connection,
                                 "treeland_prelaunch_splash_manager_v2",
                                 &treeland_prelaunch_splash_manager_v2_interface,
                                 2);
    state.manager = client_bind(&connection,
                                "treeland_foreign_toplevel_manager_v3",
                                &treeland_foreign_toplevel_manager_v3_interface,
                                1);
    shm = client_bind(&connection, "wl_shm", &wl_shm_interface, 1);
    if (!resolver_manager || !splash_manager || !state.manager || !shm)
        goto done;
    treeland_foreign_toplevel_manager_v3_add_listener(state.manager, &manager_listener, &state);

    resolver = treeland_app_id_resolver_manager_v2_get_resolver(resolver_manager);
    if (!resolver
        || treeland_app_id_resolver_v2_add_listener(resolver, &resolver_listener, &resolver_state))
        goto done;

    /* Phase 1: the splash wrapper and its teardown. */
    stage = "create splash";
    icon = create_solid_buffer(shm, ICON_WIDTH, ICON_HEIGHT, ICON_ARGB);
    if (!icon)
        goto done;
    splash = treeland_prelaunch_splash_manager_v2_create_splash(splash_manager,
                                                              splash_app_id,
                                                              "protocol-instance",
                                                              "org.deepin.Sandbox",
                                                              icon);
    if (!splash || wl_display_roundtrip(connection.display) < 0)
        goto done;

    /* The compositor owns the icon from here; the client drops its reference like AM does. */
    wl_buffer_destroy(icon);
    icon = NULL;

    if (!invoke_on_server_thread(prelaunch_splash_desktop_wait_for_creation, &observed) || !observed
        || !invoke_on_server_thread(prelaunch_splash_desktop_read_state, &desktop_state)
        || !state_has_production_splash(&desktop_state)
        || !state_has_surviving_icon(&desktop_state)) {
        stage = "splash wrapper";
        goto done;
    }
    observed = 0;

    treeland_prelaunch_splash_v2_destroy(splash);
    splash = NULL;
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(prelaunch_splash_desktop_wait_for_destruction, &observed)
        || !observed
        || !invoke_on_server_thread(prelaunch_splash_desktop_read_state, &desktop_state)
        || !desktop_state.wrapper_destroyed || desktop_state.wrapper_in_workspace) {
        stage = "splash teardown";
        goto done;
    }
    observed = 0;
    desktop_state = (struct prelaunch_splash_desktop_state){ 0 };

    /* Phase 2: the icon outliving the splash, drawn by the dock preview. */
    stage = "create icon splash";
    icon = create_solid_buffer(shm, ICON_WIDTH, ICON_HEIGHT, ICON_ARGB);
    if (!icon)
        goto done;
    splash = treeland_prelaunch_splash_manager_v2_create_splash(splash_manager,
                                                               icon_app_id,
                                                               "dockpreview-instance",
                                                               "org.deepin.Sandbox",
                                                               icon);
    if (!splash || wl_display_roundtrip(connection.display) < 0)
        goto done;
    wl_buffer_destroy(icon);
    icon = NULL;

    if (!invoke_on_server_thread(prelaunch_splash_desktop_wait_for_creation, &observed) || !observed) {
        stage = "icon splash wrapper";
        goto done;
    }
    observed = 0;

    stage = "convert splash into the app window";
    if (!map_window(&connection,
                    &resolver_state,
                    &splash_window,
                    resolver,
                    icon_app_id,
                    SPLASH_WINDOW_WIDTH,
                    SPLASH_WINDOW_HEIGHT))
        goto done;
    if (!invoke_on_server_thread(prelaunch_splash_desktop_wait_converted, &observed) || !observed
        || !invoke_on_server_thread(prelaunch_splash_desktop_read_state, &desktop_state)
        || !desktop_state.converted_to_xdg || !desktop_state.splash_item_gone
        || !state_has_surviving_icon(&desktop_state)) {
        stage = "splash conversion";
        goto done;
    }

    stage = "map a window without a splash";
    splash_identifier = wait_identifier(&connection, &state, icon_app_id);
    if (!splash_identifier)
        goto done;
    if (!map_window(&connection,
                    &resolver_state,
                    &plain_window,
                    resolver,
                    plain_app_id,
                    PLAIN_WINDOW_WIDTH,
                    PLAIN_WINDOW_HEIGHT))
        goto done;
    plain_identifier = wait_other_identifier(&connection, &state, splash_identifier);
    if (!plain_identifier)
        goto done;

    stage = "create dock preview context";
    state.context = treeland_foreign_toplevel_manager_v3_get_dock_preview_context(
        state.manager, splash_window.surface);
    if (!state.context)
        goto done;
    treeland_dock_preview_context_v3_add_listener(state.context, &context_listener, &state);

    stage = "dock preview shows the AM icon";
    if (!show_preview(&connection, &state, splash_identifier) || !probe_icon(&probe, 1)
        || !icon_drawn(&probe)) {
        report_probe("dock preview shows the AM icon", &probe);
        goto done;
    }
    report_probe("dock preview shows the AM icon", &probe);

    stage = "hover switching";
    for (int i = 0; i < PREVIEW_SWITCHES; ++i) {
        if (!show_preview(&connection, &state, plain_identifier) || !probe_icon(&probe, 0)
            || !icon_hidden(&probe)) {
            report_probe("hover to a window without a splash", &probe);
            goto done;
        }
        if (!show_preview(&connection, &state, splash_identifier) || !probe_icon(&probe, 1)
            || !icon_drawn(&probe)) {
            report_probe("hover back to the window with the icon", &probe);
            goto done;
        }
    }
    report_probe("after repeated hover switching", &probe);

    stage = "close the window the icon came from";
    xdg_toplevel_client_destroy(&splash_window);
    memset(&splash_window, 0, sizeof(splash_window));
    wl_display_roundtrip(connection.display);

    if (!show_preview(&connection, &state, plain_identifier) || !probe_icon(&probe, 0)
        || !icon_hidden(&probe)) {
        report_probe("hover after the icon's window closed", &probe);
        goto done;
    }
    report_probe("hover after the icon's window closed", &probe);

    stage = "close the dock preview";
    treeland_dock_preview_context_v3_close(state.context);
    if (wl_display_roundtrip(connection.display) < 0)
        goto done;

    result = 0;
done:
    if (result != 0) {
        fprintf(stderr,
                "prelaunch splash desktop failed at %s: splash=(created=%d xdg=%d item-gone=%d "
                "icon=%d %dx%d) destruction=(destroyed=%d in-workspace=%d) "
                "identifiers=(splash=%u plain=%u) output=%d\n",
                stage,
                desktop_state.wrapper_created,
                desktop_state.converted_to_xdg,
                desktop_state.splash_item_gone,
                desktop_state.icon_ready,
                desktop_state.icon_width,
                desktop_state.icon_height,
                desktop_state.wrapper_destroyed,
                desktop_state.wrapper_in_workspace,
                splash_identifier,
                plain_identifier,
                desktop_state.output_ready);
    }
    if (icon)
        wl_buffer_destroy(icon);
    if (state.context)
        treeland_dock_preview_context_v3_destroy(state.context);
    for (int i = 0; i < state.handle_count; ++i) {
        if (state.handles[i].handle)
            treeland_foreign_toplevel_handle_v3_destroy(state.handles[i].handle);
    }
    if (plain_window.surface)
        xdg_toplevel_client_destroy(&plain_window);
    if (splash_window.surface)
        xdg_toplevel_client_destroy(&splash_window);
    if (splash)
        treeland_prelaunch_splash_v2_destroy(splash);
    if (resolver)
        treeland_app_id_resolver_v2_destroy(resolver);
    if (splash_manager)
        treeland_prelaunch_splash_manager_v2_destroy(splash_manager);
    if (resolver_manager)
        treeland_app_id_resolver_manager_v2_destroy(resolver_manager);
    if (state.manager)
        treeland_foreign_toplevel_manager_v3_destroy(state.manager);
    if (resolver_state.pidfd >= 0)
        close(resolver_state.pidfd);
    client_disconnect(&connection);
    return result;
}
