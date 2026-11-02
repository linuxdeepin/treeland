// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#define _POSIX_C_SOURCE 200809L
#include "session-isolation.h"
#include "appearance-isolation.h"
#include "background-window.h"
#include "treeland-prelaunch-splash-unstable-v2-client-protocol.h"
#include "treeland-shortcut-manager-unstable-v3-client-protocol.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "treeland-foreign-toplevel-manager-unstable-v3-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static void shortcut_activated(void *data, struct treeland_shortcut_manager_v3 *manager,
                               const char *name, uint32_t flags)
{
    (void)data; (void)manager; (void)name; (void)flags;
}

static void shortcut_failure(void *data, struct treeland_shortcut_manager_v3 *manager,
                             const char *name, uint32_t error)
{
    (void)manager; (void)name; (void)error;
    ++*(unsigned *)data;
}

static const struct treeland_shortcut_manager_v3_listener shortcut_listener = {
    .activated = shortcut_activated,
    .bind_failure = shortcut_failure,
};

struct output_observer {
    struct zwlr_output_head_v1 *head;
    struct zwlr_output_mode_v1 *mode;
    uint32_t serial;
    int head_count;
    int head_finished_count;
    int manager_done_count;
    int manager_finished_count;
    int head_name_count;
    int head_description_count;
    int head_enabled_count;
    int head_current_mode_count;
    int head_position_count;
    int head_transform_count;
    int head_scale_count;
    int head_adaptive_sync_count;
    int mode_size_count;
    int mode_refresh_count;
    int mode_preferred_count;
    int configuration_succeeded_count;
    int configuration_failed_count;
    int configuration_cancelled_count;
    int position_x;
    int position_y;
};

static const struct zwlr_output_mode_v1_listener mode_listener;

static void head_name(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    (void)head;
    (void)name;
    ((struct output_observer *)data)->head_name_count++;
}

static void head_description(void *data, struct zwlr_output_head_v1 *head, const char *description)
{
    (void)head;
    (void)description;
    ((struct output_observer *)data)->head_description_count++;
}

static void head_physical_size(void *data, struct zwlr_output_head_v1 *head, int32_t width, int32_t height)
{
    (void)data;
    (void)head;
    (void)width;
    (void)height;
}

static void head_mode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)head;
    struct output_observer *events = data;
    events->mode = mode;
    zwlr_output_mode_v1_add_listener(mode, &mode_listener, events);
}

static void head_enabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    (void)head;
    (void)enabled;
    ((struct output_observer *)data)->head_enabled_count++;
}

static void head_current_mode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *mode)
{
    (void)head;
    (void)mode;
    ((struct output_observer *)data)->head_current_mode_count++;
}

static void head_position(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y)
{
    (void)head;
    struct output_observer *events = data;
    events->position_x = x;
    events->position_y = y;
    events->head_position_count++;
}

static void head_transform(void *data, struct zwlr_output_head_v1 *head, int32_t transform)
{
    (void)head;
    (void)transform;
    ((struct output_observer *)data)->head_transform_count++;
}

static void head_scale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
    (void)head;
    (void)scale;
    ((struct output_observer *)data)->head_scale_count++;
}

static void head_finished(void *data, struct zwlr_output_head_v1 *head)
{
    (void)head;
    ((struct output_observer *)data)->head_finished_count++;
}

static void head_make(void *data, struct zwlr_output_head_v1 *head, const char *make)
{
    (void)data;
    (void)head;
    (void)make;
}

static void head_model(void *data, struct zwlr_output_head_v1 *head, const char *model)
{
    (void)data;
    (void)head;
    (void)model;
}

static void head_serial_number(void *data, struct zwlr_output_head_v1 *head, const char *serial_number)
{
    (void)data;
    (void)head;
    (void)serial_number;
}

static void head_adaptive_sync(void *data, struct zwlr_output_head_v1 *head, uint32_t state)
{
    (void)head;
    (void)state;
    ((struct output_observer *)data)->head_adaptive_sync_count++;
}

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = head_name,
    .description = head_description,
    .physical_size = head_physical_size,
    .mode = head_mode,
    .enabled = head_enabled,
    .current_mode = head_current_mode,
    .position = head_position,
    .transform = head_transform,
    .scale = head_scale,
    .finished = head_finished,
    .make = head_make,
    .model = head_model,
    .serial_number = head_serial_number,
    .adaptive_sync = head_adaptive_sync,
};

static void mode_size(void *data, struct zwlr_output_mode_v1 *mode, int32_t width, int32_t height)
{
    (void)mode;
    (void)width;
    (void)height;
    ((struct output_observer *)data)->mode_size_count++;
}

static void mode_refresh(void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
    (void)mode;
    (void)refresh;
    ((struct output_observer *)data)->mode_refresh_count++;
}

static void mode_preferred(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)mode;
    ((struct output_observer *)data)->mode_preferred_count++;
}

static void mode_finished(void *data, struct zwlr_output_mode_v1 *mode)
{
    (void)data;
    (void)mode;
}

static const struct zwlr_output_mode_v1_listener mode_listener = {
    .size = mode_size,
    .refresh = mode_refresh,
    .preferred = mode_preferred,
    .finished = mode_finished,
};

static void manager_head(void *data, struct zwlr_output_manager_v1 *manager,
                         struct zwlr_output_head_v1 *head)
{
    (void)manager;
    struct output_observer *events = data;
    events->head = head;
    events->head_count++;
    zwlr_output_head_v1_add_listener(head, &head_listener, events);
}

static void manager_done(void *data, struct zwlr_output_manager_v1 *manager, uint32_t serial)
{
    (void)manager;
    struct output_observer *events = data;
    events->serial = serial;
    events->manager_done_count++;
}

static void manager_finished(void *data, struct zwlr_output_manager_v1 *manager)
{
    (void)manager;
    ((struct output_observer *)data)->manager_finished_count++;
}

static const struct zwlr_output_manager_v1_listener manager_listener = {
    .head = manager_head,
    .done = manager_done,
    .finished = manager_finished,
};

static int has_one_scoped_global(const struct client_connection *connection, const char *interface)
{
    unsigned int count = 0;
    for (uint32_t i = 0; i < connection->global_count; ++i) {
        if (strcmp(connection->globals[i].interface, interface) == 0)
            ++count;
    }
    return count == 1;
}

static int has_own_shell_globals(const struct client_connection *connection)
{
    return has_one_scoped_global(connection, "treeland_prelaunch_splash_manager_v2")
        && has_one_scoped_global(connection, "treeland_app_id_resolver_manager_v2")
        && has_one_scoped_global(connection, "treeland_shortcut_manager_v3")
        && has_one_scoped_global(connection, "xdg_wm_base")
        && has_one_scoped_global(connection, "zwlr_layer_shell_v1")
        && has_one_scoped_global(connection, "zxdg_decoration_manager_v1")
        && has_one_scoped_global(connection, "treeland_personalization_manager_v1")
        && has_one_scoped_global(connection, "treeland_appearance_v1")
        && has_one_scoped_global(connection, "treeland_appearance_manager_v1")
        && has_one_scoped_global(connection, "wl_data_device_manager")
        && has_one_scoped_global(connection, "zwp_primary_selection_device_manager_v1")
        && has_one_scoped_global(connection, "zwlr_data_control_manager_v1")
        && has_one_scoped_global(connection, "ext_data_control_manager_v1")
        && has_one_scoped_global(connection, "zwp_input_method_manager_v2")
        && has_one_scoped_global(connection, "zwp_text_input_manager_v1")
        && has_one_scoped_global(connection, "zwp_text_input_manager_v2")
        && has_one_scoped_global(connection, "zwp_text_input_manager_v3")
        && has_one_scoped_global(connection, "zwp_virtual_keyboard_manager_v1")
        && has_one_scoped_global(connection, "zxdg_exporter_v2")
        && has_one_scoped_global(connection, "zxdg_importer_v2")
        && has_one_scoped_global(connection, "xdg_activation_v1");
}

static int has_independent_capture_globals(const struct client_connection *a,
                                           const struct client_connection *b)
{
    static const char *interfaces[] = {
        "zwlr_screencopy_manager_v1",
        "ext_image_copy_capture_manager_v1",
        "ext_output_image_capture_source_manager_v1",
        "ext_foreign_toplevel_image_capture_source_manager_v1",
    };
    for (unsigned i = 0; i < sizeof(interfaces) / sizeof(interfaces[0]); ++i) {
        if (!has_one_scoped_global(a, interfaces[i]) || !has_one_scoped_global(b, interfaces[i]))
            return 0;
        uint32_t a_name = 0, b_name = 0;
        for (uint32_t j = 0; j < a->global_count; ++j) {
            if (strcmp(a->globals[j].interface, interfaces[i]) == 0)
                a_name = a->globals[j].name;
        }
        for (uint32_t j = 0; j < b->global_count; ++j) {
            if (strcmp(b->globals[j].interface, interfaces[i]) == 0)
                b_name = b->globals[j].name;
        }
        if (!a_name || !b_name || a_name == b_name)
            return 0;
    }
    return 1;
}

struct panel {
    struct zwlr_layer_shell_v1 *shell;
    struct zwlr_layer_surface_v1 *layer;
    struct wl_surface *surface;
    struct wl_buffer *buffer;
    uint32_t serial;
};

static void panel_configure(void *data, struct zwlr_layer_surface_v1 *layer,
                            uint32_t serial, uint32_t width, uint32_t height)
{
    (void)width; (void)height;
    ((struct panel *)data)->serial = serial;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
}

static void panel_closed(void *data, struct zwlr_layer_surface_v1 *layer)
{
    (void)data; (void)layer;
}

static const struct zwlr_layer_surface_v1_listener panel_listener = {
    .configure = panel_configure, .closed = panel_closed
};

static int panel_create(struct client_connection *connection,
                        struct xdg_toplevel_client *window, struct panel *panel, int height)
{
    panel->shell = client_bind(connection, "zwlr_layer_shell_v1", &zwlr_layer_shell_v1_interface, 1);
    if (!panel->shell)
        return 0;
    panel->surface = wl_compositor_create_surface(window->compositor);
    panel->layer = zwlr_layer_shell_v1_get_layer_surface(panel->shell, panel->surface,
                    NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "session-panel");
    zwlr_layer_surface_v1_add_listener(panel->layer, &panel_listener, panel);
    zwlr_layer_surface_v1_set_size(panel->layer, 320, height);
    zwlr_layer_surface_v1_set_anchor(panel->layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_exclusive_zone(panel->layer, height);
    wl_surface_commit(panel->surface);
    if (wl_display_roundtrip(connection->display) < 0 || !panel->serial)
        return 0;
    char name[] = "/treeland-panel-XXXXXX";
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    int size = 320 * height * 4;
    if (ftruncate(fd, size) < 0) { close(fd); return 0; }
    void *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) { close(fd); return 0; }
    memset(pixels, 0xff, size);
    struct wl_shm_pool *pool = wl_shm_create_pool(window->shm, fd, size);
    panel->buffer = wl_shm_pool_create_buffer(pool, 0, 320, height, 320 * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(pixels, size);
    close(fd);
    wl_surface_attach(panel->surface, panel->buffer, 0, 0);
    wl_surface_damage(panel->surface, 0, 0, 320, height);
    wl_surface_commit(panel->surface);
    return wl_display_roundtrip(connection->display) >= 0;
}

static void panel_destroy(struct panel *panel)
{
    if (panel->layer) zwlr_layer_surface_v1_destroy(panel->layer);
    if (panel->surface) wl_surface_destroy(panel->surface);
    if (panel->buffer) wl_buffer_destroy(panel->buffer);
    if (panel->shell) zwlr_layer_shell_v1_destroy(panel->shell);
}

struct observer {
    struct treeland_foreign_toplevel_manager_v3 *custom;
    struct zwlr_foreign_toplevel_manager_v1 *wlr;
    struct ext_foreign_toplevel_list_v1 *ext;
    struct treeland_foreign_toplevel_handle_v3 *custom_handle;
    struct zwlr_foreign_toplevel_handle_v1 *wlr_handle;
    struct ext_foreign_toplevel_handle_v1 *ext_handle;
    int custom_count;
    int wlr_count;
    int ext_count;
    uint32_t identifier;
    const char *expected_title;
    unsigned int title_mask;
    int leaked_title;
};

static void custom_title(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, const char *value)
{
    (void)handle;
    struct observer *observer = data;
    if (strcmp(value, "session-A") == 0 || strcmp(value, "session-B") == 0) {
        if (strcmp(value, observer->expected_title) != 0)
            observer->leaked_title = 1;
        else
            observer->title_mask |= 1;
    }
}

static void custom_app_id(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, const char *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void custom_pid(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, uint32_t value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void custom_identifier(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, uint32_t value)
{
    (void)handle;
    ((struct observer *)data)->identifier = value;
}

static void custom_output_enter(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, struct wl_output *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void custom_output_leave(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, struct wl_output *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void custom_state(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, struct wl_array *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void custom_done(void *data, struct treeland_foreign_toplevel_handle_v3 *handle)
{
    (void)handle;
    (void)data;
}

static void custom_closed(void *data, struct treeland_foreign_toplevel_handle_v3 *handle)
{
    (void)handle;
    (void)data;
}

static void custom_parent(void *data, struct treeland_foreign_toplevel_handle_v3 *handle, struct treeland_foreign_toplevel_handle_v3 *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static const struct treeland_foreign_toplevel_handle_v3_listener custom_handle_listener = {
    .title = custom_title,
    .app_id = custom_app_id,
    .pid = custom_pid,
    .identifier = custom_identifier,
    .output_enter = custom_output_enter,
    .output_leave = custom_output_leave,
    .state = custom_state,
    .done = custom_done,
    .closed = custom_closed,
    .parent = custom_parent,
};

static void wlr_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, const char *value)
{
    (void)handle;
    struct observer *observer = data;
    if (strcmp(value, "session-A") == 0 || strcmp(value, "session-B") == 0) {
        if (strcmp(value, observer->expected_title) != 0)
            observer->leaked_title = 1;
        else
            observer->title_mask |= 2;
    }
}

static void wlr_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, const char *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void wlr_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct wl_output *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void wlr_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct wl_output *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void wlr_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct wl_array *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void wlr_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)handle;
    (void)data;
}

static void wlr_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)handle;
    (void)data;
}

static void wlr_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, struct zwlr_foreign_toplevel_handle_v1 *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener wlr_handle_listener = {
    .title = wlr_title,
    .app_id = wlr_app_id,
    .output_enter = wlr_output_enter,
    .output_leave = wlr_output_leave,
    .state = wlr_state,
    .done = wlr_done,
    .closed = wlr_closed,
    .parent = wlr_parent,
};

static void ext_closed(void *data, struct ext_foreign_toplevel_handle_v1 *handle)
{
    (void)handle;
    (void)data;
}

static void ext_done(void *data, struct ext_foreign_toplevel_handle_v1 *handle)
{
    (void)handle;
    (void)data;
}

static void ext_title(void *data, struct ext_foreign_toplevel_handle_v1 *handle, const char *value)
{
    (void)handle;
    struct observer *observer = data;
    if (strcmp(value, "session-A") == 0 || strcmp(value, "session-B") == 0) {
        if (strcmp(value, observer->expected_title) != 0)
            observer->leaked_title = 1;
        else
            observer->title_mask |= 4;
    }
}

static void ext_app_id(void *data, struct ext_foreign_toplevel_handle_v1 *handle, const char *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static void ext_identifier(void *data, struct ext_foreign_toplevel_handle_v1 *handle, const char *value)
{
    (void)handle;
    (void)data;
    (void)value;
}

static const struct ext_foreign_toplevel_handle_v1_listener ext_handle_listener = {
    .closed = ext_closed,
    .done = ext_done,
    .title = ext_title,
    .app_id = ext_app_id,
    .identifier = ext_identifier,
};

static void custom_toplevel(void *data, struct treeland_foreign_toplevel_manager_v3 *manager,
                             struct treeland_foreign_toplevel_handle_v3 *handle)
{
    (void)manager;
    struct observer *observer = data;
    ++observer->custom_count;
    observer->custom_handle = handle;
    treeland_foreign_toplevel_handle_v3_add_listener(handle, &custom_handle_listener, observer);
}

static void custom_finished(void *data, struct treeland_foreign_toplevel_manager_v3 *manager)
{
    (void)data;
    (void)manager;
}

static const struct treeland_foreign_toplevel_manager_v3_listener custom_listener = {
    .toplevel = custom_toplevel,
    .finished = custom_finished,
};

static void wlr_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager,
                             struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)manager;
    struct observer *observer = data;
    ++observer->wlr_count;
    observer->wlr_handle = handle;
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &wlr_handle_listener, observer);
}

static void wlr_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager)
{
    (void)data;
    (void)manager;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener wlr_listener = {
    .toplevel = wlr_toplevel,
    .finished = wlr_finished,
};

static void ext_toplevel(void *data, struct ext_foreign_toplevel_list_v1 *manager,
                             struct ext_foreign_toplevel_handle_v1 *handle)
{
    (void)manager;
    struct observer *observer = data;
    ++observer->ext_count;
    observer->ext_handle = handle;
    ext_foreign_toplevel_handle_v1_add_listener(handle, &ext_handle_listener, observer);
}

static void ext_finished(void *data, struct ext_foreign_toplevel_list_v1 *manager)
{
    (void)data;
    (void)manager;
}

static const struct ext_foreign_toplevel_list_v1_listener ext_listener = {
    .toplevel = ext_toplevel,
    .finished = ext_finished,
};

static void preview_event(void *data, struct treeland_dock_preview_context_v3 *context)
{
    (void)data;
    (void)context;
}

static const struct treeland_dock_preview_context_v3_listener preview_listener = {
    .enter = preview_event,
    .leave = preview_event,
};

static int observe(struct client_connection *connection, struct observer *observer)
{
    observer->custom = client_bind(connection, "treeland_foreign_toplevel_manager_v3",
        &treeland_foreign_toplevel_manager_v3_interface, 1);
    observer->wlr = client_bind(connection, "zwlr_foreign_toplevel_manager_v1",
        &zwlr_foreign_toplevel_manager_v1_interface, 1);
    observer->ext = client_bind(connection, "ext_foreign_toplevel_list_v1",
        &ext_foreign_toplevel_list_v1_interface, 1);
    if (!observer->custom || !observer->wlr || !observer->ext)
        return 0;
    treeland_foreign_toplevel_manager_v3_add_listener(observer->custom, &custom_listener, observer);
    zwlr_foreign_toplevel_manager_v1_add_listener(observer->wlr, &wlr_listener, observer);
    ext_foreign_toplevel_list_v1_add_listener(observer->ext, &ext_listener, observer);
    return wl_display_roundtrip(connection->display) >= 0;
}

static int sees_one(const struct observer *observer)
{
    return observer->custom_count == 1 && observer->wlr_count == 1 && observer->ext_count == 1
        && observer->title_mask == 7 && !observer->leaked_title;
}

#define REQUIRE(condition, name) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", name); goto cleanup; } \
    printf("PASS: %s\n", name); \
} while (0)

static int test_prelaunch_isolation(struct client_connection *a, struct client_connection *b)
{
    struct treeland_prelaunch_splash_manager_v2 *am = client_bind(a,
        "treeland_prelaunch_splash_manager_v2", &treeland_prelaunch_splash_manager_v2_interface, 1);
    struct treeland_prelaunch_splash_manager_v2 *bm = client_bind(b,
        "treeland_prelaunch_splash_manager_v2", &treeland_prelaunch_splash_manager_v2_interface, 1);
    struct treeland_prelaunch_splash_v2 *as = NULL, *bs = NULL;
    struct isolation_state state = {0};
    int result = 1;
    if (!am || !bm)
        goto cleanup;
    as = treeland_prelaunch_splash_manager_v2_create_splash(am,
        "org.deepin.session-launch-test", "same-instance", "", NULL);
    bs = treeland_prelaunch_splash_manager_v2_create_splash(bm,
        "org.deepin.session-launch-test", "same-instance", "", NULL);
    if (!as || !bs || wl_display_roundtrip(a->display) < 0 || wl_display_roundtrip(b->display) < 0
        || !invoke_on_server_thread(isolation_read, &state)
        || state.a_windows != 1 || state.b_windows != 1 || !state.independent_protocol_domains || !state.protocol_socket_ownership
        || state.a_visible_windows != 1 || state.b_visible_windows != 0)
        goto cleanup;
    treeland_prelaunch_splash_v2_destroy(bs);
    bs = NULL;
    if (wl_display_roundtrip(b->display) < 0
        || !invoke_on_server_thread(isolation_read, &state)
        || state.a_windows != 1 || state.b_windows != 0)
        goto cleanup;
    result = 0;
cleanup:
    if (as) treeland_prelaunch_splash_v2_destroy(as);
    if (bs) treeland_prelaunch_splash_v2_destroy(bs);
    if (am) treeland_prelaunch_splash_manager_v2_destroy(am);
    if (bm) treeland_prelaunch_splash_manager_v2_destroy(bm);
    if (wl_display_roundtrip(a->display) < 0 || wl_display_roundtrip(b->display) < 0)
        result = 1;
    return result;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection a = {0}, b = {0};
    struct xdg_toplevel_client a_window = {0}, b_window = {0};
    struct panel a_panel = {0}, b_panel = {0};
    struct observer a_live = {.expected_title = "session-A"};
    struct observer b_live = {.expected_title = "session-B"};
    struct observer a_late = {.expected_title = "session-A"};
    struct observer b_late = {.expected_title = "session-B"};
    struct isolation_state state = {0};
    struct treeland_dock_preview_context_v3 *preview = NULL;
    int result = 1;
    int switched = 0;
    struct treeland_shortcut_manager_v3 *a_shortcuts = NULL, *b_shortcuts = NULL;
    unsigned a_shortcut_failures = 0, b_shortcut_failures = 0;
    struct zwlr_output_manager_v1 *a_outputs = NULL, *b_outputs = NULL;
    struct output_observer a_heads = {0}, b_heads = {0};
    int b_scale_events_before = 0;

    REQUIRE(invoke_on_server_thread(isolation_read, &state), "read session fixture");
    if (!state.ready) {
        fprintf(stderr, "SKIP: no second local account available\n");
        return 77;
    }
    REQUIRE(client_connect(&a, socket_name) && client_connect(&b, state.socket_name),
            "connect two independent sessions");
    REQUIRE(test_prelaunch_isolation(&a, &b) == 0,
            "same appId/instanceId splash requests and closure remain inside each Session");
    REQUIRE(observe(&a, &a_live) && observe(&b, &b_live), "bind before windows exist");
    REQUIRE(has_own_shell_globals(&a) && has_own_shell_globals(&b),
            "each connection sees only its own shell and personalization globals");
    REQUIRE(has_independent_capture_globals(&a, &b),
            "each Session sees one distinct global for each upstream capture manager");
    REQUIRE(session_test_appearance_isolation(&a, &b) == 0,
            "appearance requests and broadcasts stay inside the owning Session");
    a_shortcuts = client_bind(&a, "treeland_shortcut_manager_v3", &treeland_shortcut_manager_v3_interface, 1);
    b_shortcuts = client_bind(&b, "treeland_shortcut_manager_v3", &treeland_shortcut_manager_v3_interface, 1);
    REQUIRE(a_shortcuts && b_shortcuts, "bind independent shortcut managers");
    treeland_shortcut_manager_v3_add_listener(a_shortcuts, &shortcut_listener, &a_shortcut_failures);
    treeland_shortcut_manager_v3_add_listener(b_shortcuts, &shortcut_listener, &b_shortcut_failures);
    treeland_shortcut_manager_v3_acquire(a_shortcuts);
    treeland_shortcut_manager_v3_acquire(b_shortcuts);
    treeland_shortcut_manager_v3_bind_key(a_shortcuts, "shared-name", "F9",
        TREELAND_SHORTCUT_MANAGER_V3_KEYBIND_FLAG_KEY_PRESS, TREELAND_SHORTCUT_MANAGER_V3_ACTION_NOTIFY);
    treeland_shortcut_manager_v3_bind_key(b_shortcuts, "shared-name", "F10",
        TREELAND_SHORTCUT_MANAGER_V3_KEYBIND_FLAG_KEY_PRESS, TREELAND_SHORTCUT_MANAGER_V3_ACTION_NOTIFY);
    REQUIRE(wl_display_roundtrip(a.display) >= 0 && wl_display_roundtrip(b.display) >= 0
            && !a_shortcut_failures && !b_shortcut_failures
            && invoke_on_server_thread(isolation_read, &state)
            && state.a_shortcut && state.b_shortcut && !state.foreign_shortcut
            && state.inactive_surfaces_stopped,
            "inactive B binds immediately without changing A's shortcut");
    treeland_shortcut_manager_v3_bind_key(b_shortcuts, "renamed", "F10",
        TREELAND_SHORTCUT_MANAGER_V3_KEYBIND_FLAG_KEY_PRESS, TREELAND_SHORTCUT_MANAGER_V3_ACTION_NOTIFY);
    treeland_shortcut_manager_v3_unbind(b_shortcuts, "renamed");
    REQUIRE(wl_display_roundtrip(b.display) >= 0 && !b_shortcut_failures
            && invoke_on_server_thread(isolation_read, &state)
            && state.a_shortcut && !state.b_shortcut && !state.foreign_shortcut,
            "inactive upsert followed by unbind leaves no stale binding");
    treeland_shortcut_manager_v3_bind_key(b_shortcuts, "shared-name", "F10",
        TREELAND_SHORTCUT_MANAGER_V3_KEYBIND_FLAG_KEY_PRESS, TREELAND_SHORTCUT_MANAGER_V3_ACTION_NOTIFY);
    REQUIRE(wl_display_roundtrip(b.display) >= 0 && !b_shortcut_failures,
            "restore B's binding before activation");
    a_outputs = client_bind(&a, "zwlr_output_manager_v1", &zwlr_output_manager_v1_interface, 4);
    b_outputs = client_bind(&b, "zwlr_output_manager_v1", &zwlr_output_manager_v1_interface, 4);
    REQUIRE(a_outputs && b_outputs, "bind independent output managers");
    zwlr_output_manager_v1_add_listener(a_outputs, &manager_listener, &a_heads);
    zwlr_output_manager_v1_add_listener(b_outputs, &manager_listener, &b_heads);
    REQUIRE(wl_display_roundtrip(a.display) >= 0 && wl_display_roundtrip(b.display) >= 0
            && a_heads.head_count == 3 && b_heads.head_count == 3,
            "each manager advertises the three connected screens exactly once");
    REQUIRE(xdg_toplevel_client_create(&a, &a_window)
            && xdg_toplevel_client_create(&b, &b_window), "map one window in each session");
    xdg_toplevel_set_app_id(a_window.toplevel, "org.deepin.session-same-app");
    xdg_toplevel_set_app_id(b_window.toplevel, "org.deepin.session-same-app");
    xdg_toplevel_set_title(a_window.toplevel, "session-A");
    xdg_toplevel_set_title(b_window.toplevel, "session-B");
    REQUIRE(wl_display_roundtrip(a.display) >= 0 && wl_display_roundtrip(b.display) >= 0,
            "synchronize both window streams");
    REQUIRE(sees_one(&a_live) && sees_one(&b_live), "live enumeration isolates all three protocols");
    REQUIRE(observe(&a, &a_late) && observe(&b, &b_late), "bind after both windows exist");
    REQUIRE(sees_one(&a_late) && sees_one(&b_late), "initial enumeration isolates all three protocols");
    REQUIRE(invoke_on_server_thread(isolation_read, &state)
            && state.a_windows == 1 && state.b_windows == 1 && state.independent_workspaces
            && state.independent_desktops && state.independent_managers && state.independent_protocol_domains
            && state.independent_seats && state.protocol_socket_ownership && state.active_managers && state.inactive_surfaces_stopped,
            "Session owns independent window models and workspaces");

    b_scale_events_before = b_heads.head_scale_count;
    REQUIRE(invoke_on_server_thread(isolation_scale_a, NULL)
            && wl_display_roundtrip(a.display) >= 0
            && invoke_on_server_thread(isolation_read, &state)
            && state.a_scale == 1.5 && state.b_scale == 1.5 && state.hardware_scale == 1.5
            && state.b_logical_width == state.a_logical_width,
            "system scale is shared while Session window models remain independent");
    REQUIRE(wl_display_roundtrip(b.display) >= 0
            && a_heads.head_count == 3 && b_heads.head_count == 3
            && b_heads.head_scale_count > b_scale_events_before,
            "system scale change updates B's existing output protocol heads");

    REQUIRE(panel_create(&a, &a_window, &a_panel, 40)
            && panel_create(&b, &b_window, &b_panel, 60), "map panels in both sessions");
    REQUIRE(invoke_on_server_thread(isolation_read, &state)
            && state.independent_outputs && state.a_bottom_zone == 40 && state.b_bottom_zone == 60
            && state.active_bottom_zone == 40 && state.a_panel_at_bottom && state.b_panel_at_bottom
            && state.inactive_surfaces_stopped,
            "logical outputs isolate panel placement and exclusive zones");

    REQUIRE(invoke_on_server_thread(isolation_switch_b, NULL), "activate session B");
    switched = 1;
    REQUIRE(invoke_on_server_thread(isolation_read, &state)
            && state.a_current_workspace == 1 && state.b_current_workspace == 0 && state.active_managers
            && state.a_shortcut && state.b_shortcut && !state.foreign_shortcut
            && state.inactive_surfaces_stopped,
            "switch preserves each session's workspace selection");
    REQUIRE(invoke_on_server_thread(isolation_read, &state)
            && state.active_bottom_zone == 60 && state.a_bottom_zone == 40
            && state.b_bottom_zone == 60 && state.a_panel_at_bottom && state.b_panel_at_bottom,
            "switch projects session B's exclusive zone without stacking panels");
    REQUIRE(invoke_on_server_thread(isolation_read, &state)
            && state.a_scale == 1.5 && state.b_scale == 1.5 && state.hardware_scale == 1.5
            && state.b_logical_width == state.a_logical_width,
            "switch to B preserves the system scale and shared screen geometry");
    REQUIRE(wl_display_roundtrip(a.display) >= 0 && wl_display_roundtrip(b.display) >= 0
            && a_heads.head_count == 3 && b_heads.head_count == 3
            && !a_heads.head_finished_count && !b_heads.head_finished_count,
            "account switch updates existing heads without adding or finishing screens");
    preview = treeland_foreign_toplevel_manager_v3_get_dock_preview_context(b_live.custom, b_window.surface);
    treeland_dock_preview_context_v3_add_listener(preview, &preview_listener, NULL);
    struct wl_array identifiers;
    wl_array_init(&identifiers);
    uint32_t *identifier = wl_array_add(&identifiers, sizeof(*identifier));
    if (!identifier) { wl_array_release(&identifiers); goto cleanup; }
    *identifier = a_live.identifier;
    treeland_dock_preview_context_v3_show(preview, &identifiers, 0, 0,
                                         TREELAND_DOCK_PREVIEW_CONTEXT_V3_DIRECTION_BOTTOM);
    wl_array_release(&identifiers);
    REQUIRE(wl_display_roundtrip(b.display) >= 0
            && invoke_on_server_thread(isolation_read, &state)
            && state.preview_requests == 1 && state.foreign_preview_surfaces == 0,
            "guessed identifier from another session cannot preview a window");
    result = 0;

cleanup:
    if (preview) treeland_dock_preview_context_v3_destroy(preview);
    if (b.display) {
        panel_destroy(&b_panel);
        xdg_toplevel_client_destroy(&b_window);
        client_disconnect(&b);
    }
    if (switched) {
        if (!invoke_on_server_thread(isolation_restore_a, NULL)) result = 1;
        if (!invoke_on_server_thread(isolation_read, &state)
            || state.a_current_workspace != 1 || state.active_bottom_zone != 40 || state.hardware_scale != 1.5
            || !state.a_shortcut || state.foreign_shortcut) result = 1;
        if (result == 0 && session_test_background_window() != 0) {
            fprintf(stderr, "FAIL: background child-socket window did not restore across users\n");
            result = 1;
        }
        if (result == 0) {
            for (int i = 0; i < 3; ++i) {
                if (!invoke_on_server_thread(isolation_reactivate_b, NULL)
                    || !invoke_on_server_thread(isolation_restore_a, NULL)
                    || wl_display_roundtrip(a.display) < 0
                    || a_heads.head_count != 3 || a_heads.head_finished_count != 0) {
                    fprintf(stderr, "FAIL: repeated account switches grew the output head list\n");
                    result = 1;
                    break;
                }
            }
        }
    }
    if (a.display) {
        panel_destroy(&a_panel);
        xdg_toplevel_client_destroy(&a_window);
        client_disconnect(&a);
    }
    return result;
}
