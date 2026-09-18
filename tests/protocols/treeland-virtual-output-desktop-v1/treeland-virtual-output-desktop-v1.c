// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#define _POSIX_C_SOURCE 200809L
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-virtual-output-desktop-v1.h"
#include "treeland-virtual-output-manager-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void virtual_output_desktop_read_state(void *data);
extern void virtual_output_desktop_render(void *data);

/* ---- layer-shell surface bound to HEADLESS-2 ---- */

struct layer_events {
    unsigned int configure;
    unsigned int closed;
    uint32_t serial;
};

static void layer_configure(void *data,
                            struct zwlr_layer_surface_v1 *surface,
                            uint32_t serial,
                            uint32_t width,
                            uint32_t height)
{
    (void)surface;
    (void)width;
    (void)height;
    struct layer_events *events = data;
    events->serial = serial;
    events->configure++;
}

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
    (void)surface;
    ((struct layer_events *)data)->closed++;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

static int make_buffer(struct wl_shm *shm, struct wl_buffer **buffer)
{
    const int width = 640;
    const int height = 480;
    const int stride = width * 4;
    const int size = stride * height;
    char name[] = "/treeland-vod-XXXXXX";
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return 0;
    }
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        close(fd);
        return 0;
    }
    memset(p, 0xff, size);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    *buffer = pool
        ? wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888)
        : NULL;
    if (pool)
        wl_shm_pool_destroy(pool);
    munmap(p, size);
    close(fd);
    return *buffer != NULL;
}

/* ---- wlr-output-management: heads and configuration ---- */

#define MAX_HEADS 8

struct head_entry {
    struct zwlr_output_head_v1 *head;
    char name[64];
};

static struct head_entry s_heads[MAX_HEADS];
static uint32_t s_head_count;
static uint32_t s_manager_serial;
static unsigned int s_config_succeeded;
static unsigned int s_config_failed;

/* wlroots aborts on NULL head/managers listeners ("listener function for
 * opcode N ... is NULL"), so every callback must be provided even when the
 * test does not assert its payload. */
static void head_name(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    (void)data;
    for (uint32_t i = 0; i < s_head_count; ++i) {
        if (s_heads[i].head == head) {
            snprintf(s_heads[i].name, sizeof(s_heads[i].name), "%s", name ? name : "");
            break;
        }
    }
}

static void head_noop(void *data, struct zwlr_output_head_v1 *head)
{
    (void)data;
    (void)head;
}

static void head_physical_size(void *data, struct zwlr_output_head_v1 *head, int32_t width,
                               int32_t height)
{
    (void)data;
    (void)head;
    (void)width;
    (void)height;
}

static void head_mode_event(void *data, struct zwlr_output_head_v1 *head,
                            struct zwlr_output_mode_v1 *mode)
{
    (void)data;
    (void)head;
    (void)mode;
}

static void head_enabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    (void)data;
    (void)head;
    (void)enabled;
}

static void head_position(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y)
{
    (void)data;
    (void)head;
    (void)x;
    (void)y;
}

static void head_transform(void *data, struct zwlr_output_head_v1 *head, int32_t transform)
{
    (void)data;
    (void)head;
    (void)transform;
}

static void head_scale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
    (void)data;
    (void)head;
    (void)scale;
}

static void head_adaptive_sync(void *data, struct zwlr_output_head_v1 *head, uint32_t state)
{
    (void)data;
    (void)head;
    (void)state;
}

static void head_string(void *data, struct zwlr_output_head_v1 *head, const char *string)
{
    (void)data;
    (void)head;
    (void)string;
}

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = head_name,
    .description = head_string,
    .physical_size = head_physical_size,
    .mode = head_mode_event,
    .enabled = head_enabled,
    .current_mode = head_mode_event,
    .position = head_position,
    .transform = head_transform,
    .scale = head_scale,
    .finished = head_noop,
    .make = head_string,
    .model = head_string,
    .serial_number = head_string,
    .adaptive_sync = head_adaptive_sync,
};

static void manager_head(void *data,
                         struct zwlr_output_manager_v1 *manager,
                         struct zwlr_output_head_v1 *head)
{
    (void)data;
    (void)manager;
    if (s_head_count < MAX_HEADS) {
        s_heads[s_head_count].head = head;
        s_heads[s_head_count].name[0] = '\0';
        s_head_count++;
        zwlr_output_head_v1_add_listener(head, &head_listener, NULL);
    }
}

static void manager_done(void *data, struct zwlr_output_manager_v1 *manager, uint32_t serial)
{
    (void)data;
    (void)manager;
    s_manager_serial = serial;
}

static void manager_finished(void *data, struct zwlr_output_manager_v1 *manager)
{
    (void)data;
    (void)manager;
}

static const struct zwlr_output_manager_v1_listener manager_listener = {
    .head = manager_head,
    .done = manager_done,
    .finished = manager_finished,
};

static void config_succeeded(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)data;
    (void)configuration;
    s_config_succeeded++;
}

static void config_failed(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)data;
    (void)configuration;
    s_config_failed++;
}

static void config_cancelled(void *data, struct zwlr_output_configuration_v1 *configuration)
{
    (void)data;
    (void)configuration;
}

static const struct zwlr_output_configuration_v1_listener config_listener = {
    .succeeded = config_succeeded,
    .failed = config_failed,
    .cancelled = config_cancelled,
};

static struct zwlr_output_head_v1 *find_head(const char *name)
{
    for (uint32_t i = 0; i < s_head_count; ++i) {
        if (strcmp(s_heads[i].name, name) == 0)
            return s_heads[i].head;
    }
    return NULL;
}

/* ---- virtual output manager group ---- */

struct group_events {
    int outputs_received;
    char group_name[64];
    char outputs[128];
};

static void group_outputs(void *data,
                          struct treeland_virtual_output_v1 *group,
                          const char *name,
                          struct wl_array *outputs)
{
    (void)group;
    struct group_events *events = data;
    events->outputs_received++;
    snprintf(events->group_name, sizeof(events->group_name), "%s", name ? name : "");
    const char *cursor = outputs->data;
    const char *end = cursor + outputs->size;
    size_t used = 0;
    events->outputs[0] = '\0';
    while (cursor < end && *cursor) {
        const size_t length = strlen(cursor);
        if (used + length + 2 > sizeof(events->outputs))
            break;
        if (used)
            events->outputs[used++] = ' ';
        memcpy(events->outputs + used, cursor, length);
        used += length;
        events->outputs[used] = '\0';
        cursor += length + 1;
    }
}

static void group_error(void *data,
                        struct treeland_virtual_output_v1 *group,
                        uint32_t code,
                        const char *message)
{
    (void)data;
    (void)group;
    (void)code;
    (void)message;
}

static const struct treeland_virtual_output_v1_listener group_listener = {
    .outputs = group_outputs,
    .error = group_error,
};

static int fill_outputs(struct wl_array *array)
{
    static const char *const names[] = { "HEADLESS-1", "HEADLESS-2" };
    wl_array_init(array);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const size_t length = strlen(names[i]) + 1;
        char *slot = wl_array_add(array, length);
        if (!slot) {
            wl_array_release(array);
            return 0;
        }
        memcpy(slot, names[i], length);
    }
    return 1;
}

/* ---- helpers ---- */

/* Bind the second wl_output global. The desktop fixture registers HEADLESS-1
 * during startup and setup.cpp adds HEADLESS-2 right after, so registry order
 * is deterministic; the layer surface must live on HEADLESS-2 because that is
 * the output that gets re-wrapped by every copy<->normal conversion below. */
static struct wl_output *bind_second_output(struct client_connection *connection)
{
    uint32_t seen = 0;
    for (uint32_t i = 0; i < connection->global_count; ++i) {
        if (strcmp(connection->globals[i].interface, "wl_output") == 0) {
            if (++seen == 2) {
                const uint32_t version = connection->globals[i].version < 4
                    ? connection->globals[i].version
                    : 4;
                return wl_registry_bind(connection->registry,
                                        connection->globals[i].name,
                                        &wl_output_interface,
                                        version);
            }
        }
    }
    return NULL;
}

static void cleanup(struct client_connection *connection,
                    struct treeland_virtual_output_manager_v1 *manager,
                    struct treeland_virtual_output_v1 *group,
                    struct wl_compositor *compositor,
                    struct wl_shm *shm,
                    struct zwlr_layer_shell_v1 *shell,
                    struct zwlr_layer_surface_v1 *layer,
                    struct wl_surface *surface,
                    struct wl_buffer *buffer,
                    struct wl_output *output2,
                    struct zwlr_output_manager_v1 *output_manager)
{
    if (layer)
        zwlr_layer_surface_v1_destroy(layer);
    if (shell)
        zwlr_layer_shell_v1_destroy(shell);
    if (buffer)
        wl_buffer_destroy(buffer);
    if (surface)
        wl_surface_destroy(surface);
    if (output2)
        wl_output_destroy(output2);
    if (compositor)
        wl_compositor_destroy(compositor);
    if (shm)
        wl_shm_destroy(shm);
    if (group)
        treeland_virtual_output_v1_destroy(group);
    if (manager)
        treeland_virtual_output_manager_v1_destroy(manager);
    if (output_manager)
        wl_proxy_destroy((struct wl_proxy *)output_manager);
    client_disconnect(connection);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection = { 0 };
    struct treeland_virtual_output_manager_v1 *manager = NULL;
    struct treeland_virtual_output_v1 *group = NULL;
    struct wl_compositor *compositor = NULL;
    struct wl_shm *shm = NULL;
    struct zwlr_layer_shell_v1 *shell = NULL;
    struct zwlr_layer_surface_v1 *layer = NULL;
    struct wl_surface *surface = NULL;
    struct wl_buffer *buffer = NULL;
    struct wl_output *output2 = NULL;
    struct zwlr_output_manager_v1 *output_manager = NULL;
    struct group_events events = { 0 };
    struct layer_events layer_ev = { 0 };
    struct virtual_output_desktop_state before = { 0 };
    struct virtual_output_desktop_state after_layer = { 0 };
    struct virtual_output_desktop_state copied = { 0 };
    struct virtual_output_desktop_state restored = { 0 };
    struct virtual_output_desktop_state recopied = { 0 };
    struct virtual_output_desktop_state final = { 0 };
    int stage = 0;
    int result = 1;

    if (!client_connect(&connection, socket_name))
        goto done;
    manager = client_bind(&connection,
                          "treeland_virtual_output_manager_v1",
                          &treeland_virtual_output_manager_v1_interface,
                          2);
    if (!manager
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &before)
        || !before.first_present || !before.second_present
        || before.root_output_count != 2 || !before.first_is_source || !before.second_is_source)
        goto done;

    /* Bind a layer-shell surface to HEADLESS-2 (the output that becomes the
     * copy/mirror). It must survive every copy<->normal wrapper swap below:
     * entering copy mode, restoring it, and collapsing copy mode by
     * disabling the source output. */
    stage = 1;
    compositor = client_bind(&connection, "wl_compositor", &wl_compositor_interface, 4);
    shm = client_bind(&connection, "wl_shm", &wl_shm_interface, 1);
    shell = client_bind(&connection,
                        "zwlr_layer_shell_v1",
                        &zwlr_layer_shell_v1_interface,
                        5);
    output2 = bind_second_output(&connection);
    if (!compositor || !shm || !shell || !output2)
        goto done;
    surface = wl_compositor_create_surface(compositor);
    layer = zwlr_layer_shell_v1_get_layer_surface(shell,
                                                  surface,
                                                  output2,
                                                  ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
                                                  "virtual-output-desktop-test");
    if (!surface || !layer)
        goto done;
    zwlr_layer_surface_v1_add_listener(layer, &layer_listener, &layer_ev);
    zwlr_layer_surface_v1_set_size(layer, 640, 480);
    wl_surface_commit(surface);

    stage = 2;
    if (wl_display_roundtrip(connection.display) < 0 || !layer_ev.configure || !layer_ev.serial)
        goto done;
    zwlr_layer_surface_v1_ack_configure(layer, layer_ev.serial);
    if (!make_buffer(shm, &buffer))
        goto done;
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, 640, 480);
    wl_surface_commit(surface);

    stage = 3;
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &after_layer)
        || !after_layer.layer_alive || !after_layer.layer_on_second
        || !after_layer.layer_container_on_second || layer_ev.closed != 0)
        goto done;

    /* Enter copy mode: HEADLESS-1 is the source, HEADLESS-2 becomes the copy. */
    stage = 4;
    struct wl_array outputs;
    if (!fill_outputs(&outputs))
        goto done;
    group = treeland_virtual_output_manager_v1_create_virtual_output(
        manager, "protocol-copy-group", &outputs);
    wl_array_release(&outputs);
    if (!group)
        goto done;
    treeland_virtual_output_v1_add_listener(group, &group_listener, &events);

    stage = 5;
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &copied))
        goto done;
    if (events.outputs_received < 1 || strcmp(events.group_name, "protocol-copy-group") != 0
        || strcmp(events.outputs, "HEADLESS-1 HEADLESS-2") != 0
        || !copied.first_present || !copied.second_present || copied.root_output_count != 2
        || !copied.primary_is_first || !copied.first_is_source || !copied.second_is_copy
        || !copied.layer_alive || !copied.layer_on_second || !copied.layer_container_on_second
        || layer_ev.closed != 0)
        goto done;

    /* Restore: the copy wrapper of HEADLESS-2 is replaced by a normal one. */
    stage = 6;
    treeland_virtual_output_v1_destroy(group);
    group = NULL;
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &restored))
        goto done;
    if (!restored.first_present || !restored.second_present || restored.root_output_count != 2
        || !restored.primary_is_first || !restored.first_is_source || !restored.second_is_source
        || !restored.layer_alive || !restored.layer_on_second
        || !restored.layer_container_on_second || layer_ev.closed != 0)
        goto done;

    /* Re-enter copy mode, then disable the source (HEADLESS-1). This collapses
     * copy mode: HEADLESS-2 is converted back to a normal output while its
     * layer-shell surface must stay alive and re-entered, instead of being
     * closed as if the physical output had been unplugged. */
    stage = 7;
    if (!fill_outputs(&outputs))
        goto done;
    group = treeland_virtual_output_manager_v1_create_virtual_output(
        manager, "protocol-copy-group-2", &outputs);
    wl_array_release(&outputs);
    if (!group)
        goto done;
    treeland_virtual_output_v1_add_listener(group, &group_listener, &events);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &recopied))
        goto done;
    if (!recopied.first_present || !recopied.second_present || !recopied.primary_is_first
        || !recopied.first_is_source || !recopied.second_is_copy
        || !recopied.layer_alive || !recopied.layer_on_second
        || !recopied.layer_container_on_second || layer_ev.closed != 0)
        goto done;

    stage = 8;
    output_manager = client_bind(&connection,
                                 "zwlr_output_manager_v1",
                                 &zwlr_output_manager_v1_interface,
                                 4);
    if (!output_manager)
        goto done;
    zwlr_output_manager_v1_add_listener(output_manager, &manager_listener, NULL);
    if (wl_display_roundtrip(connection.display) < 0 || s_head_count < 2 || !s_manager_serial)
        goto done;
    struct zwlr_output_head_v1 *source_head = find_head("HEADLESS-1");
    if (!source_head)
        goto done;
    struct zwlr_output_configuration_v1 *configuration =
        zwlr_output_manager_v1_create_configuration(output_manager, s_manager_serial);
    if (!configuration)
        goto done;
    zwlr_output_configuration_v1_add_listener(configuration, &config_listener, NULL);
    zwlr_output_configuration_v1_disable_head(configuration, source_head);
    stage = 9;
    zwlr_output_configuration_v1_apply(configuration);
    if (wl_display_roundtrip(connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_render, NULL)
        || wl_display_roundtrip(connection.display) < 0
        || s_config_succeeded + s_config_failed < 1)
        goto done;
    if (s_config_succeeded != 1 || s_config_failed != 0)
        goto done;
    zwlr_output_configuration_v1_destroy(configuration);
    configuration = NULL;
    if (!invoke_on_server_thread(virtual_output_desktop_read_state, &final))
        goto done;
    if (!final.first_present || !final.first_disabled || !final.second_present
        || final.root_output_count != 2 || final.primary_is_first || !final.first_is_source
        || !final.second_is_source || final.second_is_copy || !final.layer_alive
        || !final.layer_on_second || !final.layer_container_on_second || layer_ev.closed != 0)
        goto done;

    result = 0;
done:
    if (result != 0) {
        fprintf(stderr,
                "virtual output desktop failed at stage %d: before=(%d,%d root=%d source=%d,%d) "
                "after-layer=(alive=%d on2=%d ctr2=%d) "
                "copied=(%d,%d root=%d primary=%d source=%d mirror=%d event=%d) "
                "restored=(%d,%d root=%d source=%d,%d alive=%d) "
                "final=(present=%d,%d root=%d primary-first=%d first-disabled=%d "
                "first-source=%d second-source=%d second-copy=%d alive=%d on2=%d ctr2=%d) "
                "layer=(configure=%u closed=%u) config=(succeeded=%u failed=%u) heads=%u\n",
                stage,
                before.first_present, before.second_present, before.root_output_count,
                before.first_is_source, before.second_is_source,
                after_layer.layer_alive, after_layer.layer_on_second,
                after_layer.layer_container_on_second,
                copied.first_present, copied.second_present, copied.root_output_count,
                copied.primary_is_first, copied.first_is_source, copied.second_is_copy,
                events.outputs_received,
                restored.first_present, restored.second_present, restored.root_output_count,
                restored.first_is_source, restored.second_is_source, restored.layer_alive,
                final.first_present, final.second_present, final.root_output_count,
                final.primary_is_first, final.first_disabled, final.first_is_source,
                final.second_is_source, final.second_is_copy, final.layer_alive,
                final.layer_on_second, final.layer_container_on_second,
                layer_ev.configure, layer_ev.closed, s_config_succeeded, s_config_failed,
                s_head_count);
    }
    cleanup(&connection,
            manager,
            group,
            compositor,
            shm,
            shell,
            layer,
            surface,
            buffer,
            output2,
            output_manager);
    return result;
}
