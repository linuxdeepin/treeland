// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "xdg-toplevel-client.h"
#include "server-bridge-api.h"
#include "treeland-capture-desktop-v1.h"
#include "treeland-capture-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void capture_desktop_select_mapped_surface(void *data);
extern void capture_desktop_render_selected_source(void *data);

struct capture_client {
    struct client_connection connection;
    struct xdg_toplevel_client toplevel;
    struct treeland_capture_manager_v1 *manager;
    struct treeland_capture_context_v1 *context;
    struct treeland_capture_frame_v1 *frame;
    struct wl_buffer *target_buffer;
    void *target_data;
    size_t target_size;
    uint32_t target_format;
    uint32_t target_width;
    uint32_t target_height;
    uint32_t target_stride;
    int source_ready;
    int source_failed;
    uint32_t source_type;
    uint32_t source_width;
    uint32_t source_height;
    int buffer_received;
    int buffer_done;
    int frame_ready;
    int frame_failed;
};

static void context_source_ready(void *data,
                                 struct treeland_capture_context_v1 *context,
                                 int32_t x,
                                 int32_t y,
                                 uint32_t width,
                                 uint32_t height,
                                 uint32_t source_type)
{
    (void)context;
    (void)x;
    (void)y;
    struct capture_client *client = data;
    client->source_ready = 1;
    client->source_width = width;
    client->source_height = height;
    client->source_type = source_type;
}

static void context_source_failed(void *data,
                                  struct treeland_capture_context_v1 *context,
                                  uint32_t reason)
{
    (void)context;
    (void)reason;
    ((struct capture_client *)data)->source_failed = 1;
}

static const struct treeland_capture_context_v1_listener context_listener = {
    .source_ready = context_source_ready,
    .source_failed = context_source_failed,
};

static int create_target_buffer(struct capture_client *client)
{
    const size_t size = (size_t)client->target_stride * client->target_height;
    char name[64];
    snprintf(name, sizeof(name), "/treeland_capture_target_%d", (int)getpid());
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return 0;
    }
    client->target_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (client->target_data == MAP_FAILED) {
        client->target_data = NULL;
        close(fd);
        return 0;
    }
    memset(client->target_data, 0, size);
    struct wl_shm_pool *pool = wl_shm_create_pool(client->toplevel.shm, fd, (int)size);
    close(fd);
    if (!pool)
        return 0;
    client->target_buffer = wl_shm_pool_create_buffer(pool,
                                                       0,
                                                       (int)client->target_width,
                                                       (int)client->target_height,
                                                       (int)client->target_stride,
                                                       client->target_format);
    wl_shm_pool_destroy(pool);
    if (!client->target_buffer)
        return 0;
    client->target_size = size;
    return 1;
}

static void frame_buffer(void *data,
                         struct treeland_capture_frame_v1 *frame,
                         uint32_t format,
                         uint32_t width,
                         uint32_t height,
                         uint32_t stride)
{
    struct capture_client *client = data;
    if (client->buffer_received)
        return;
    client->target_format = format;
    client->target_width = width;
    client->target_height = height;
    client->target_stride = stride;
    client->buffer_received = create_target_buffer(client);
    if (client->buffer_received) {
        treeland_capture_frame_v1_copy(frame, client->target_buffer);
        wl_display_flush(client->connection.display);
    }
}

static void frame_buffer_done(void *data, struct treeland_capture_frame_v1 *frame)
{
    (void)frame;
    ((struct capture_client *)data)->buffer_done = 1;
}

static void frame_flags(void *data, struct treeland_capture_frame_v1 *frame, uint32_t flags)
{
    (void)data;
    (void)frame;
    (void)flags;
}

static void frame_ready(void *data, struct treeland_capture_frame_v1 *frame)
{
    (void)frame;
    ((struct capture_client *)data)->frame_ready = 1;
}

static void frame_failed(void *data, struct treeland_capture_frame_v1 *frame)
{
    (void)frame;
    ((struct capture_client *)data)->frame_failed = 1;
}

static const struct treeland_capture_frame_v1_listener frame_listener = {
    .buffer = frame_buffer,
    .buffer_done = frame_buffer_done,
    .flags = frame_flags,
    .ready = frame_ready,
    .failed = frame_failed,
};

static int target_is_opaque_red(const struct capture_client *client)
{
    if (!client->target_data || client->target_width != 64 || client->target_height != 64
        || client->target_stride != 64 * 4)
        return 0;
    const uint32_t pixel = *(const uint32_t *)client->target_data;
    switch (client->target_format) {
    case WL_SHM_FORMAT_ARGB8888:
        return ((pixel >> 24) & 0xff) == 255 && ((pixel >> 16) & 0xff) == 255
               && ((pixel >> 8) & 0xff) == 0 && (pixel & 0xff) == 0;
    case WL_SHM_FORMAT_ABGR8888:
        return ((pixel >> 24) & 0xff) == 255 && (pixel & 0xff) == 255
               && ((pixel >> 8) & 0xff) == 0 && ((pixel >> 16) & 0xff) == 0;
    default:
        return 0;
    }
}

static void cleanup(struct capture_client *client)
{
    if (client->frame)
        treeland_capture_frame_v1_destroy(client->frame);
    if (client->context)
        treeland_capture_context_v1_destroy(client->context);
    if (client->manager)
        treeland_capture_manager_v1_destroy(client->manager);
    if (client->target_buffer)
        wl_buffer_destroy(client->target_buffer);
    if (client->target_data)
        munmap(client->target_data, client->target_size);
    xdg_toplevel_client_destroy(&client->toplevel);
    client_disconnect(&client->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct capture_client client = { 0 };
    struct capture_desktop_selection_state selection = { 0 };
    struct capture_desktop_selection_state render = { 0 };
    int result = 1;

    if (!client_connect(&client.connection, socket_name)
        || !xdg_toplevel_client_create_with_solid_buffer(
            &client.connection, &client.toplevel, 64, 64, 0xffff0000u))
        goto done;
    client.manager = client_bind(&client.connection,
                                        "treeland_capture_manager_v1",
                                        &treeland_capture_manager_v1_interface,
                                        1);
    if (!client.manager)
        goto done;
    client.context = treeland_capture_manager_v1_get_context(client.manager);
    if (!client.context)
        goto done;
    treeland_capture_context_v1_add_listener(client.context, &context_listener, &client);

    treeland_capture_context_v1_select_source(client.context,
                                               TREELAND_CAPTURE_CONTEXT_V1_SOURCE_TYPE_WINDOW,
                                               0,
                                               0,
                                               NULL);
    if (wl_display_roundtrip(client.connection.display) < 0
        || !invoke_on_server_thread(capture_desktop_select_mapped_surface, &selection)
        || wl_display_roundtrip(client.connection.display) < 0
        || !selection.output_ready || !selection.wrapper_ready || !selection.surface_content_ready
        || !selection.content_in_paint_order || !selection.content_visible
        || selection.content_width != 64 || selection.content_height != 64
        || !selection.selector_ready || !selection.hovered_mapped_content
        || !selection.source_selected || !selection.source_is_surface
        || selection.source_width != 64 || selection.source_height != 64
        || !client.source_ready || client.source_failed
        || client.source_type != TREELAND_CAPTURE_CONTEXT_V1_SOURCE_TYPE_WINDOW
        || client.source_width != 64 || client.source_height != 64)
        goto done;

    client.frame = treeland_capture_context_v1_capture(client.context);
    if (!client.frame)
        goto done;
    treeland_capture_frame_v1_add_listener(client.frame, &frame_listener, &client);

    // The roundtrip is a protocol ordering barrier: it proves onCapture() and
    // CaptureSourceSelector::doneSelection() have run before we request the
    // real output render that produces the source image.
    if (wl_display_roundtrip(client.connection.display) < 0
        || !invoke_on_server_thread(capture_desktop_render_selected_source, &render)
        || !render.render_requested)
        goto done;

    while (!client.frame_ready && !client.frame_failed) {
        if (wl_display_dispatch(client.connection.display) < 0)
            goto done;
    }
    if (!client.frame_failed && client.buffer_received && client.buffer_done && target_is_opaque_red(&client))
        result = 0;

done:
    if (result != 0) {
        fprintf(stderr,
                "capture desktop failed: content=(paint=%d visible=%d %dx%d) selector=(%d,%d,%d) source=(surface=%d %dx%d) "
                "event=(ready=%d failed=%d type=%u %ux%u) buffer=(%d done=%d fmt=0x%x %ux%u stride=%u)\n",
                selection.content_in_paint_order,
                selection.content_visible,
                selection.content_width,
                selection.content_height,
                selection.selector_ready,
                selection.hovered_mapped_content,
                selection.source_selected,
                selection.source_is_surface,
                selection.source_width,
                selection.source_height,
                client.source_ready,
                client.source_failed,
                client.source_type,
                client.source_width,
                client.source_height,
                client.buffer_received,
                client.buffer_done,
                client.target_format,
                client.target_width,
                client.target_height,
                client.target_stride);
    }
    cleanup(&client);
    return result;
}
