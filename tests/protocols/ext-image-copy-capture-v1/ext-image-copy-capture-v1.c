// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"

#include <stdio.h>

struct session_info {
    int buffer_size_received;
    uint32_t width;
    uint32_t height;
    int done;
    int stopped;
};

static void handle_buffer_size(void *data,
                              struct ext_image_copy_capture_session_v1 *s,
                              uint32_t width, uint32_t height)
{
    (void)s;
    struct session_info *info = data;
    info->buffer_size_received = 1;
    info->width = width;
    info->height = height;
}

static void handle_shm_format(void *data,
                             struct ext_image_copy_capture_session_v1 *s,
                             uint32_t format)
{
    (void)data; (void)s; (void)format;
}

static void handle_dmabuf_device(void *data,
                                struct ext_image_copy_capture_session_v1 *s,
                                struct wl_array *device)
{
    (void)data; (void)s; (void)device;
}

static void handle_dmabuf_format(void *data,
                                struct ext_image_copy_capture_session_v1 *s,
                                uint32_t format, struct wl_array *modifiers)
{
    (void)data; (void)s; (void)format; (void)modifiers;
}

static void handle_done(void *data, struct ext_image_copy_capture_session_v1 *s)
{
    (void)s;
    ((struct session_info *)data)->done = 1;
}

static void handle_stopped(void *data, struct ext_image_copy_capture_session_v1 *s)
{
    (void)s;
    ((struct session_info *)data)->stopped = 1;
}

static const struct ext_image_copy_capture_session_v1_listener listener = {
    .buffer_size = handle_buffer_size,
    .shm_format = handle_shm_format,
    .dmabuf_device = handle_dmabuf_device,
    .dmabuf_format = handle_dmabuf_format,
    .done = handle_done,
    .stopped = handle_stopped,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_output *output =
        client_bind(&conn, "wl_output", &wl_output_interface, 1);
    if (!output) {
        fprintf(stderr, "ext-image-copy-capture: failed to bind wl_output\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_output_image_capture_source_manager_v1 *src_mgr =
        client_bind(&conn, "ext_output_image_capture_source_manager_v1",
                    &ext_output_image_capture_source_manager_v1_interface, 1);
    if (!src_mgr) {
        fprintf(stderr, "ext-image-copy-capture: failed to bind source manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_image_capture_source_v1 *source =
        ext_output_image_capture_source_manager_v1_create_source(src_mgr, output);
    if (!source) {
        fprintf(stderr, "ext-image-copy-capture: create_source returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_image_copy_capture_session_v1 *session = NULL;
    struct ext_image_copy_capture_manager_v1 *cc_mgr =
        client_bind(&conn, "ext_image_copy_capture_manager_v1",
                    &ext_image_copy_capture_manager_v1_interface, 1);
    if (!cc_mgr) {
        fprintf(stderr, "ext-image-copy-capture: failed to bind copy capture manager\n");
        goto fail;
    }

    session =
        ext_image_copy_capture_manager_v1_create_session(cc_mgr, source, 0);
    if (!session) {
        fprintf(stderr, "ext-image-copy-capture: create_session returned null\n");
        goto fail;
    }

    struct session_info info = {0};
    ext_image_copy_capture_session_v1_add_listener(session, &listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-image-copy-capture: roundtrip failed\n");
        goto fail;
    }

    if (!info.buffer_size_received || info.width == 0 || info.height == 0) {
        fprintf(stderr, "ext-image-copy-capture: did not receive valid buffer_size\n");
        goto fail;
    }

    if (!info.done) {
        fprintf(stderr, "ext-image-copy-capture: did not receive done event\n");
        goto fail;
    }

    ext_image_copy_capture_session_v1_destroy(session);
    ext_image_copy_capture_manager_v1_destroy(cc_mgr);
    ext_image_capture_source_v1_destroy(source);
    ext_output_image_capture_source_manager_v1_destroy(src_mgr);
    client_disconnect(&conn);
    return 0;

fail:
    if (session)
        ext_image_copy_capture_session_v1_destroy(session);
    if (cc_mgr)
        ext_image_copy_capture_manager_v1_destroy(cc_mgr);
    ext_image_capture_source_v1_destroy(source);
    ext_output_image_capture_source_manager_v1_destroy(src_mgr);
    client_disconnect(&conn);
    return 1;
}
