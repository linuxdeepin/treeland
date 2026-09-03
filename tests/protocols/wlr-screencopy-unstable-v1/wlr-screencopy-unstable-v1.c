// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#define _POSIX_C_SOURCE 200809L

#include "client-connection.h"
#include "screencopy-test.h"
#include "server-bridge-api.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void screencopy_render(void *data);

struct frame_events
{
    uint32_t format, width, height, stride, flags;
    int buffer, done, flags_seen, ready, failed, damage_seen;
};

struct layer_events
{
    uint32_t serial, width, height;
    int configured;
};

static void layer_configure(void *data,
                            struct zwlr_layer_surface_v1 *surface,
                            uint32_t serial,
                            uint32_t width,
                            uint32_t height)
{
    (void)surface;
    struct layer_events *events = data;
    events->serial = serial;
    events->width = width;
    events->height = height;
    events->configured++;
}

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
    (void)data;
    (void)surface;
}
static const struct zwlr_layer_surface_v1_listener layer_listener = { .configure = layer_configure,
                                                                      .closed = layer_closed };

static void frame_buffer(void *data,
                         struct zwlr_screencopy_frame_v1 *frame,
                         uint32_t format,
                         uint32_t width,
                         uint32_t height,
                         uint32_t stride)
{
    (void)frame;
    struct frame_events *events = data;
    events->format = format;
    events->width = width;
    events->height = height;
    events->stride = stride;
    events->buffer++;
}

static void frame_flags(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags)
{
    (void)frame;
    struct frame_events *events = data;
    events->flags = flags;
    events->flags_seen++;
}

static void frame_ready(void *data,
                        struct zwlr_screencopy_frame_v1 *frame,
                        uint32_t hi,
                        uint32_t lo,
                        uint32_t nsec)
{
    (void)frame;
    (void)hi;
    (void)lo;
    (void)nsec;
    ((struct frame_events *)data)->ready++;
}

static void frame_failed(void *data, struct zwlr_screencopy_frame_v1 *frame)
{
    (void)frame;
    ((struct frame_events *)data)->failed++;
}

static void frame_damage(void *data,
                         struct zwlr_screencopy_frame_v1 *frame,
                         uint32_t x,
                         uint32_t y,
                         uint32_t w,
                         uint32_t h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    ((struct frame_events *)data)->damage_seen++;
}

static void
frame_dmabuf(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t f, uint32_t w, uint32_t h)
{
    (void)data;
    (void)frame;
    (void)f;
    (void)w;
    (void)h;
}

static void frame_done(void *data, struct zwlr_screencopy_frame_v1 *frame)
{
    (void)frame;
    ((struct frame_events *)data)->done++;
}
static const struct zwlr_screencopy_frame_v1_listener frame_listener = { .buffer = frame_buffer,
                                                                         .flags = frame_flags,
                                                                         .ready = frame_ready,
                                                                         .failed = frame_failed,
                                                                         .damage = frame_damage,
                                                                         .linux_dmabuf =
                                                                             frame_dmabuf,
                                                                         .buffer_done =
                                                                             frame_done };

static int make_buffer(struct wl_shm *shm,
                       uint32_t format,
                       int width,
                       int height,
                       int stride,
                       struct wl_buffer **buffer,
                       void **pixels,
                       size_t *size)
{
    char name[64];
    snprintf(name, sizeof(name), "/treeland_screencopy_%d", (int)getpid());
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    *size = (size_t)stride * height;
    if (ftruncate(fd, (off_t)*size) < 0) {
        close(fd);
        return 0;
    }
    *pixels = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*pixels == MAP_FAILED) {
        close(fd);
        return 0;
    }
    memset(*pixels, 0, *size);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int)*size);
    close(fd);
    *buffer = pool ? wl_shm_pool_create_buffer(pool, 0, width, height, stride, format) : NULL;
    if (pool)
        wl_shm_pool_destroy(pool);
    if (!*buffer) {
        munmap(*pixels, *size);
        return 0;
    }
    return 1;
}

static int capture(struct client_connection *connection,
                   struct zwlr_screencopy_manager_v1 *manager,
                   struct wl_output *output,
                   struct wl_shm *shm,
                   int region,
                   int with_damage,
                   int region_x,
                   int region_y)
{
    struct frame_events events = { 0 };
    struct screencopy_render_state render_state = { 0 };
    struct zwlr_screencopy_frame_v1 *frame = region
        ? zwlr_screencopy_manager_v1_capture_output_region(manager,
                                                           0,
                                                           output,
                                                           region_x,
                                                           region_y,
                                                           64,
                                                           64)
        : zwlr_screencopy_manager_v1_capture_output(manager, 0, output);
    if (!frame)
        return 0;

    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, &events);
    if (wl_display_roundtrip(connection->display) < 0 || events.buffer != 1 || events.done != 1
        || !events.width || !events.height || !events.stride)
        goto fail;
    struct wl_buffer *buffer = NULL;
    void *pixels = NULL;
    size_t size = 0;
    if (!make_buffer(shm,
                     events.format,
                     events.width,
                     events.height,
                     events.stride,
                     &buffer,
                     &pixels,
                     &size))
        goto fail;
    if (with_damage)
        zwlr_screencopy_frame_v1_copy_with_damage(frame, buffer);
    else
        zwlr_screencopy_frame_v1_copy(frame, buffer);
    // The first roundtrip is a protocol barrier: copy() has been handled and
    // wlroots has installed its output-commit listener before we render.
    if (wl_display_roundtrip(connection->display) < 0
        || !invoke_on_server_thread(screencopy_render, &render_state)
        || wl_display_roundtrip(connection->display) < 0 || events.ready != 1 || events.failed
        || events.flags_seen != 1 || (with_damage && !events.damage_seen)) {
        wl_buffer_destroy(buffer);
        munmap(pixels, size);
        goto fail;
    }
    const uint32_t sample =
        ((uint32_t *)((char *)pixels + (events.height / 2) * events.stride))[events.width / 2];
    wl_buffer_destroy(buffer);
    munmap(pixels, size);
    zwlr_screencopy_frame_v1_destroy(frame);
    if ((sample & 0x00ffffffu) == 0x00ff0000u)
        return 1;
    fprintf(stderr,
            "wlr-screencopy pixel failure: region=%d damage=%d sample=%#x size=%ux%u stride=%u\n",
            region,
            with_damage,
            sample,
            events.width,
            events.height,
            events.stride);
    return 0;
fail:
    fprintf(stderr,
            "wlr-screencopy frame failure: region=%d damage=%d events=(buffer=%d done=%d ready=%d "
            "failed=%d flags=%d damage=%d size=%ux%u stride=%u)\n",
            region,
            with_damage,
            events.buffer,
            events.done,
            events.ready,
            events.failed,
            events.flags_seen,
            events.damage_seen,
            events.width,
            events.height,
            events.stride);
    fprintf(stderr,
            "wlr-screencopy render state: outputs=%d enabled=%d->%d needs-frame=%d->%d "
            "frame-pending=%d->%d attach-render-locks=%d->%d render-end=%d target-committed=%d\n",
            render_state.output_count,
            render_state.output_enabled_before,
            render_state.output_enabled_after,
            render_state.needs_frame_before,
            render_state.needs_frame_after,
            render_state.frame_pending_before,
            render_state.frame_pending_after,
            render_state.attach_render_locks_before,
            render_state.attach_render_locks_after,
            render_state.render_end_count,
            render_state.target_committed);
    zwlr_screencopy_frame_v1_destroy(frame);
    return 0;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;
    struct wl_compositor *compositor =
        client_bind(&connection, "wl_compositor", &wl_compositor_interface, 1);
    struct wl_shm *shm = client_bind(&connection, "wl_shm", &wl_shm_interface, 1);
    struct wl_output *output = client_bind(&connection, "wl_output", &wl_output_interface, 1);
    struct zwlr_layer_shell_v1 *layer_shell =
        client_bind(&connection, "zwlr_layer_shell_v1", &zwlr_layer_shell_v1_interface, 5);
    struct zwlr_screencopy_manager_v1 *manager = client_bind(&connection,
                                                             "zwlr_screencopy_manager_v1",
                                                             &zwlr_screencopy_manager_v1_interface,
                                                             3);
    struct wl_surface *surface = NULL;
    struct zwlr_layer_surface_v1 *layer = NULL;
    struct wl_buffer *red = NULL;
    void *red_pixels = NULL;
    size_t red_size = 0;
    struct layer_events layer_events = { 0 };
    int result = 1;
    if (!compositor || !shm || !output || !layer_shell || !manager)
        goto done;
    surface = wl_compositor_create_surface(compositor);
    layer = zwlr_layer_shell_v1_get_layer_surface(layer_shell,
                                                  surface,
                                                  output,
                                                  ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
                                                  "screencopy-test");
    if (!surface || !layer)
        goto done;
    zwlr_layer_surface_v1_add_listener(layer, &layer_listener, &layer_events);
    zwlr_layer_surface_v1_set_size(layer, 1920, 1080);
    zwlr_layer_surface_v1_set_anchor(
        layer,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
            | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    wl_surface_commit(surface);
    if (wl_display_roundtrip(connection.display) < 0 || !layer_events.configured
        || layer_events.width < 64 || layer_events.height < 64
        || !make_buffer(shm,
                        WL_SHM_FORMAT_XRGB8888,
                        layer_events.width,
                        layer_events.height,
                        layer_events.width * 4,
                        &red,
                        &red_pixels,
                        &red_size))
        goto done;
    memset(red_pixels, 0, red_size);
    for (size_t i = 0; i < red_size / 4; ++i)
        ((uint32_t *)red_pixels)[i] = 0x00ff0000u;
    zwlr_layer_surface_v1_ack_configure(layer, layer_events.serial);
    wl_surface_attach(surface, red, 0, 0);
    wl_surface_damage(surface, 0, 0, layer_events.width, layer_events.height);
    wl_surface_commit(surface);
    const int region_x = (int)(layer_events.width - 64) / 2;
    const int region_y = (int)(layer_events.height - 64) / 2;
    if (wl_display_roundtrip(connection.display) < 0
        || !capture(&connection, manager, output, shm, 0, 0, 0, 0)
        || !capture(&connection, manager, output, shm, 1, 0, region_x, region_y)
        || !capture(&connection, manager, output, shm, 1, 1, region_x, region_y))
        goto done;
    result = 0;
done:
    if (layer)
        zwlr_layer_surface_v1_destroy(layer);
    if (red)
        wl_buffer_destroy(red);
    if (red_pixels)
        munmap(red_pixels, red_size);
    if (surface)
        wl_surface_destroy(surface);
    if (manager)
        zwlr_screencopy_manager_v1_destroy(manager);
    if (layer_shell)
        zwlr_layer_shell_v1_destroy(layer_shell);
    if (output)
        wl_output_destroy(output);
    if (shm)
        wl_shm_destroy(shm);
    if (compositor)
        wl_compositor_destroy(compositor);
    client_disconnect(&connection);
    return result;
}
