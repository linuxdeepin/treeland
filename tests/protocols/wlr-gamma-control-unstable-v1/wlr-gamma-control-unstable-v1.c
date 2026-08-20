// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "client-connection.h"
#include "wlr-gamma-control-unstable-v1-client-protocol.h"

#include <stdio.h>
#ifdef TREELAND_PROTOCOL_EXPECT_DRM_GAMMA
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#endif

struct gamma_events {
    unsigned int gamma_size_count;
    unsigned int failed_count;
    uint32_t gamma_size;
};

static void gamma_size(void *data, struct zwlr_gamma_control_v1 *control, uint32_t size)
{
    (void)control;
    struct gamma_events *events = data;
    events->gamma_size = size;
    events->gamma_size_count++;
}

static void failed(void *data, struct zwlr_gamma_control_v1 *control)
{
    (void)control;
    ((struct gamma_events *)data)->failed_count++;
}

static const struct zwlr_gamma_control_v1_listener control_listener = {
    .gamma_size = gamma_size,
    .failed = failed,
};

#ifdef TREELAND_PROTOCOL_EXPECT_DRM_GAMMA
static int create_gamma_lut(uint32_t size)
{
    char path[] = "/tmp/treeland-gamma-lut-XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0)
        return -1;
    unlink(path);
    const size_t entries = (size_t)size * 3;
    uint16_t *lut = calloc(entries, sizeof(*lut));
    if (!lut) {
        close(fd);
        return -1;
    }
    for (uint32_t index = 0; index < size; ++index) {
        // A non-identity, monotonic LUT proves Helper forwards all three
        // channels to wlr_output_commit_state rather than only accepting FD.
        const uint16_t value = (uint16_t)(((uint64_t)index * UINT16_MAX) / (size - 1));
        lut[index] = value;
        lut[size + index] = (uint16_t)(UINT16_MAX - value);
        lut[2 * size + index] = value / 2;
    }
    const size_t bytes = entries * sizeof(*lut);
    size_t total_written = 0;
    while (total_written < bytes) {
        const ssize_t written = write(fd, (const char *)lut + total_written, bytes - total_written);
        if (written <= 0)
            break;
        total_written += (size_t)written;
    }
    free(lut);
    if (total_written != bytes || lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}
#endif

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    struct zwlr_gamma_control_manager_v1 *manager = client_bind(
        &connection, "zwlr_gamma_control_manager_v1", &zwlr_gamma_control_manager_v1_interface, 1);
    struct wl_output *output = client_bind(&connection, "wl_output", &wl_output_interface, 1);
    if (!manager || !output) {
        fprintf(stderr, "wlr-gamma-control: manager or headless output missing\n");
        goto failed;
    }

    struct gamma_events events = { 0 };
    struct zwlr_gamma_control_v1 *control =
        zwlr_gamma_control_manager_v1_get_gamma_control(manager, output);
    if (!control) {
        fprintf(stderr, "wlr-gamma-control: could not create control proxy\n");
        goto failed;
    }
    zwlr_gamma_control_v1_add_listener(control, &control_listener, &events);
#ifdef TREELAND_PROTOCOL_EXPECT_DRM_GAMMA
    if (wl_display_roundtrip(connection.display) < 0) {
        zwlr_gamma_control_v1_destroy(control);
        goto failed;
    }
    if (events.failed_count || events.gamma_size_count != 1 || events.gamma_size < 2) {
        fprintf(stderr, "wlr-gamma-control: DRM output has no usable gamma LUT; skipped\n");
        zwlr_gamma_control_v1_destroy(control);
        zwlr_gamma_control_manager_v1_destroy(manager);
        wl_output_destroy(output);
        client_disconnect(&connection);
        return 77;
    }
    const int lut_fd = create_gamma_lut(events.gamma_size);
    if (lut_fd < 0) {
        fprintf(stderr, "wlr-gamma-control: could not allocate LUT; skipped\n");
        zwlr_gamma_control_v1_destroy(control);
        zwlr_gamma_control_manager_v1_destroy(manager);
        wl_output_destroy(output);
        client_disconnect(&connection);
        return 77;
    }
    zwlr_gamma_control_v1_set_gamma(control, lut_fd);
    if (wl_display_roundtrip(connection.display) < 0 || events.failed_count) {
        fprintf(stderr, "wlr-gamma-control: Helper failed to commit DRM gamma LUT\n");
        zwlr_gamma_control_v1_destroy(control);
        goto failed;
    }
#else
    if (wl_display_roundtrip(connection.display) < 0 || events.gamma_size_count != 0
        || events.failed_count != 1) {
        fprintf(stderr, "wlr-gamma-control: headless output must report exactly one failed event\n");
        zwlr_gamma_control_v1_destroy(control);
        goto failed;
    }

#endif
    zwlr_gamma_control_v1_destroy(control);
    zwlr_gamma_control_manager_v1_destroy(manager);
    wl_output_destroy(output);
    client_disconnect(&connection);
    return 0;

failed:
    if (manager)
        zwlr_gamma_control_manager_v1_destroy(manager);
    if (output)
        wl_output_destroy(output);
    client_disconnect(&connection);
    return 1;
}
