// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "drm-client-protocol.h"
#include "drm.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct drm_events {
    unsigned int device_count;
    int device_nonempty;
    char device[256];
    unsigned int format_count;
    unsigned int capabilities_count;
    uint32_t capabilities;
    unsigned int authenticated_count;
};

static void handle_device(void *data, struct wl_drm *drm, const char *name)
{
    (void)drm;
    struct drm_events *events = data;
    events->device_nonempty = name && name[0] != '\0';
    if (events->device_nonempty)
        snprintf(events->device, sizeof(events->device), "%s", name);
    events->device_count++;
}

static void handle_format(void *data, struct wl_drm *drm, uint32_t format)
{
    (void)drm;
    (void)format;
    ((struct drm_events *)data)->format_count++;
}

static void handle_authenticated(void *data, struct wl_drm *drm)
{
    (void)drm;
    ((struct drm_events *)data)->authenticated_count++;
}

static void handle_capabilities(void *data, struct wl_drm *drm, uint32_t value)
{
    (void)drm;
    struct drm_events *events = data;
    events->capabilities = value;
    events->capabilities_count++;
}

static const struct wl_drm_listener drm_listener = {
    .device = handle_device,
    .format = handle_format,
    .authenticated = handle_authenticated,
    .capabilities = handle_capabilities,
};

static int has_global(const struct client_connection *connection, const char *interface)
{
    for (uint32_t i = 0; i < connection->global_count; ++i) {
        if (strcmp(connection->globals[i].interface, interface) == 0)
            return 1;
    }
    return 0;
}

static int check_initial_events(struct client_connection *connection,
                                struct wl_drm *drm,
                                struct drm_events *events)
{
    wl_drm_add_listener(drm, &drm_listener, events);
    if (wl_display_roundtrip(connection->display) < 0)
        return 0;

    return events->device_count == 1 && events->device_nonempty
           && events->capabilities_count == 1
           && events->capabilities == WL_DRM_CAPABILITY_PRIME
           && events->format_count > 0;
}

static int check_authenticate(struct client_connection *connection,
                              struct wl_drm *drm,
                              struct drm_events *events)
{
    wl_drm_authenticate(drm, 0);
    return wl_display_roundtrip(connection->display) >= 0
           && events->authenticated_count == 1;
}

static struct wl_drm *connect_fresh_drm(const char *socket_name,
                                        struct client_connection *connection)
{
    if (!client_connect(connection, socket_name))
        return NULL;
    return client_bind(connection, "wl_drm", &wl_drm_interface, 2);
}

static int check_create_buffer_rejected(const char *socket_name)
{
    struct client_connection connection;
    struct wl_drm *drm = connect_fresh_drm(socket_name, &connection);
    if (!drm)
        return 0;

    struct wl_buffer *buffer = wl_drm_create_buffer(drm, 1, 1, 1, 4,
                                                     WL_DRM_FORMAT_XRGB8888);
    (void)buffer;
    const struct wl_interface *interface = NULL;
    uint32_t code = 0;
    const int rejected = wl_display_roundtrip(connection.display) < 0
                         && wl_display_get_protocol_error(connection.display, &interface, &code)
                                == WL_DRM_ERROR_INVALID_NAME
                         && interface == &wl_drm_interface;
    client_disconnect(&connection);
    return rejected;
}

static int check_create_planar_buffer_rejected(const char *socket_name)
{
    struct client_connection connection;
    struct wl_drm *drm = connect_fresh_drm(socket_name, &connection);
    if (!drm)
        return 0;

    struct wl_buffer *buffer = wl_drm_create_planar_buffer(
        drm, 1, 1, 1, WL_DRM_FORMAT_XRGB8888, 0, 4, 0, 0, 0, 0);
    (void)buffer;
    const struct wl_interface *interface = NULL;
    uint32_t code = 0;
    const int rejected = wl_display_roundtrip(connection.display) < 0
                         && wl_display_get_protocol_error(connection.display, &interface, &code)
                                == WL_DRM_ERROR_INVALID_NAME
                         && interface == &wl_drm_interface;
    client_disconnect(&connection);
    return rejected;
}

extern void drm_read_render_state(void *data);

// Returns 1 when the renderer samples the real DMA-BUF red pixel, 0 on an
// assertion failure, and -1 when this runner cannot allocate DMA-BUF storage.
static int check_create_prime_buffer_dma(const char *socket_name, const char *device)
{
    if (!device || device[0] == '\0')
        return -1;

    const int device_fd = open(device, O_RDWR);
    if (device_fd < 0)
        return -1;
    struct gbm_device *gbm = gbm_create_device(device_fd);
    if (!gbm) {
        close(device_fd);
        return -1;
    }
    struct gbm_bo *bo = gbm_bo_create(gbm, 64, 64, DRM_FORMAT_XRGB8888,
                                      GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
    if (!bo) {
        gbm_device_destroy(gbm);
        close(device_fd);
        return -1;
    }
    int dma_fd = gbm_bo_get_fd(bo);
    if (dma_fd < 0) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        close(device_fd);
        return -1;
    }

    uint32_t map_stride = 0;
    void *map_data = NULL;
    uint32_t *pixels = gbm_bo_map(bo, 0, 0, 64, 64, GBM_BO_TRANSFER_WRITE,
                                  &map_stride, &map_data);
    if (!pixels) {
        close(dma_fd);
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        close(device_fd);
        return -1;
    }
    for (uint32_t y = 0; y < 64; ++y) {
        uint32_t *row = (uint32_t *)((char *)pixels + (size_t)y * map_stride);
        for (uint32_t x = 0; x < 64; ++x)
            row[x] = 0x00ff0000u;
    }
    gbm_bo_unmap(bo, map_data);

    struct client_connection connection;
    struct wl_drm *drm = connect_fresh_drm(socket_name, &connection);
    struct xdg_toplevel_client toplevel = { 0 };
    const int toplevel_ready = drm
        && xdg_toplevel_client_create_pending(&connection, &toplevel)
        && wl_display_roundtrip(connection.display) >= 0
        && toplevel.configured
        && xdg_toplevel_client_ack_latest_configure(&connection, &toplevel);
    struct wl_buffer *buffer = NULL;
    if (toplevel_ready) {
        buffer = wl_drm_create_prime_buffer(drm, dma_fd, 64, 64, DRM_FORMAT_XRGB8888,
                                            0, gbm_bo_get_stride(bo), 0, 0, 0, 0);
        // The request owns dma_fd once marshalled, including on a server error.
        dma_fd = -1;
    }
    if (toplevel_ready && buffer) {
        toplevel.buffer = buffer;
        wl_surface_attach(toplevel.surface, buffer, 0, 0);
        wl_surface_damage(toplevel.surface, 0, 0, 64, 64);
        wl_surface_commit(toplevel.surface);
    }
    const int committed = buffer && wl_display_roundtrip(connection.display) >= 0;
    struct drm_render_state state = { 0 };
    const int sampled = committed
        && invoke_on_server_thread(drm_read_render_state, &state)
        && state.output_ready
        && state.wrapper_created
        && state.wrapper_in_workspace
        && state.image_ready
        && state.image_width == 64
        && state.image_height == 64
        && state.sample_red == 255
        && state.sample_green == 0
        && state.sample_blue == 0
        && state.sample_alpha == 255;
    xdg_toplevel_client_destroy(&toplevel);
    if (drm)
        wl_drm_destroy(drm);
    const int destroyed = sampled && wl_display_roundtrip(connection.display) >= 0;
    client_disconnect(&connection);
    if (dma_fd >= 0)
        close(dma_fd);
    gbm_bo_destroy(bo);
    gbm_device_destroy(gbm);
    close(device_fd);
    return destroyed ? 1 : 0;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    if (!has_global(&connection, "wl_drm")) {
        fprintf(stderr, "drm: wl_drm unavailable (no DRM-capable renderer); skipped\n");
        client_disconnect(&connection);
        return 77;
    }

    struct wl_drm *drm = client_bind(&connection, "wl_drm", &wl_drm_interface, 2);
    struct drm_events events = {0};
    const int initial_events_ok = drm && check_initial_events(&connection, drm, &events);
    const int authenticate_ok = initial_events_ok && check_authenticate(&connection, drm, &events);
    if (drm)
        wl_drm_destroy(drm);
    client_disconnect(&connection);

    if (!initial_events_ok) {
        fprintf(stderr, "drm: invalid device/capabilities/format advertisement\n");
        return 1;
    }
    if (!authenticate_ok) {
        fprintf(stderr, "drm: authenticate did not send authenticated\n");
        return 1;
    }
    if (!check_create_buffer_rejected(socket_name)) {
        fprintf(stderr, "drm: create_buffer did not report invalid_name\n");
        return 1;
    }
    if (!check_create_planar_buffer_rejected(socket_name)) {
        fprintf(stderr, "drm: create_planar_buffer did not report invalid_name\n");
        return 1;
    }
    const int prime_result = check_create_prime_buffer_dma(socket_name, events.device);
    if (prime_result < 0) {
        fprintf(stderr, "drm: DMA-BUF unavailable on %s; skipped\n", events.device);
        return 77;
    }
    if (!prime_result) {
        fprintf(stderr, "drm: create_prime_buffer DMA-BUF attach/commit failed\n");
        return 1;
    }
    return 0;
}
