// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "xdg-toplevel-client.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    (void)surface;
    struct xdg_toplevel_client *toplevel = data;
    toplevel->configure_serial = serial;
    toplevel->configured = 1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                   int32_t width, int32_t height, struct wl_array *states)
{
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
    (void)states;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    (void)toplevel;
    ((struct xdg_toplevel_client *)data)->close_received = 1;
}

static void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
                                          int32_t width, int32_t height)
{
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
}

static void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
                                         struct wl_array *capabilities)
{
    (void)data;
    (void)toplevel;
    (void)capabilities;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities = xdg_toplevel_wm_capabilities,
};

int xdg_toplevel_client_ack_latest_configure(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel)
{
    if (!connection || !connection->display || !toplevel || !toplevel->xdg_surface)
        return 0;
    if (!toplevel->configure_serial || toplevel->configure_serial == toplevel->acknowledged_configure_serial)
        return 1;

    xdg_surface_ack_configure(toplevel->xdg_surface, toplevel->configure_serial);
    toplevel->acknowledged_configure_serial = toplevel->configure_serial;
    wl_surface_commit(toplevel->surface);
    return wl_display_roundtrip(connection->display) >= 0;
}

static int map_toplevel(struct client_connection *connection,
                        struct xdg_toplevel_client *toplevel,
                        int width,
                        int height,
                        uint32_t argb)
{
    if (width <= 0 || height <= 0)
        return 0;
    const int stride = width * 4;
    const size_t size = (size_t)stride * height;
    char name[64];
    snprintf(name, sizeof(name), "/treeland_protocol_xdg_%d", (int)getpid());
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
    uint32_t *pixels = data;
    for (size_t i = 0; i < (size_t)width * height; ++i)
        pixels[i] = argb;
    struct wl_shm_pool *pool = wl_shm_create_pool(toplevel->shm, fd, (int)size);
    toplevel->buffer = pool ? wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                                         WL_SHM_FORMAT_ARGB8888) : NULL;
    if (pool)
        wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    if (!toplevel->buffer)
        return 0;
    wl_surface_attach(toplevel->surface, toplevel->buffer, 0, 0);
    wl_surface_damage(toplevel->surface, 0, 0, width, height);
    wl_surface_commit(toplevel->surface);
    return wl_display_roundtrip(connection->display) >= 0;
}

static int xdg_toplevel_client_create_internal(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel,
    xdg_surface_setup_callback setup,
    void *data,
    int buffer_width,
    int buffer_height,
    uint32_t buffer_argb)
{
    memset(toplevel, 0, sizeof(*toplevel));
    toplevel->compositor = client_bind(connection, "wl_compositor", &wl_compositor_interface, 1);
    toplevel->wm_base = client_bind(connection, "xdg_wm_base", &xdg_wm_base_interface, 1);
    toplevel->shm = client_bind(connection, "wl_shm", &wl_shm_interface, 1);
    if (!toplevel->compositor || !toplevel->wm_base || !toplevel->shm)
        goto failed;

    xdg_wm_base_add_listener(toplevel->wm_base, &wm_base_listener, toplevel);
    toplevel->surface = wl_compositor_create_surface(toplevel->compositor);
    if (!toplevel->surface)
        goto failed;
    if (setup && !setup(toplevel->surface, data))
        goto failed;
    toplevel->xdg_surface = xdg_wm_base_get_xdg_surface(toplevel->wm_base, toplevel->surface);
    if (!toplevel->xdg_surface)
        goto failed;
    xdg_surface_add_listener(toplevel->xdg_surface, &xdg_surface_listener, toplevel);
    toplevel->toplevel = xdg_surface_get_toplevel(toplevel->xdg_surface);
    if (!toplevel->toplevel)
        goto failed;
    xdg_toplevel_add_listener(toplevel->toplevel, &xdg_toplevel_listener, toplevel);

    wl_surface_commit(toplevel->surface);
    if (wl_display_roundtrip(connection->display) < 0 || !toplevel->configured)
        goto failed;
    if (!xdg_toplevel_client_ack_latest_configure(connection, toplevel))
        goto failed;
    return map_toplevel(connection, toplevel, buffer_width, buffer_height, buffer_argb);

failed:
    xdg_toplevel_client_destroy(toplevel);
    return 0;
}

int xdg_toplevel_client_create_pending(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel)
{
    memset(toplevel, 0, sizeof(*toplevel));
    toplevel->compositor = client_bind(connection, "wl_compositor", &wl_compositor_interface, 1);
    toplevel->wm_base = client_bind(connection, "xdg_wm_base", &xdg_wm_base_interface, 1);
    toplevel->shm = client_bind(connection, "wl_shm", &wl_shm_interface, 1);
    if (!toplevel->compositor || !toplevel->wm_base || !toplevel->shm)
        goto failed;

    xdg_wm_base_add_listener(toplevel->wm_base, &wm_base_listener, toplevel);
    toplevel->surface = wl_compositor_create_surface(toplevel->compositor);
    if (!toplevel->surface)
        goto failed;
    toplevel->xdg_surface = xdg_wm_base_get_xdg_surface(toplevel->wm_base, toplevel->surface);
    if (!toplevel->xdg_surface)
        goto failed;
    xdg_surface_add_listener(toplevel->xdg_surface, &xdg_surface_listener, toplevel);
    toplevel->toplevel = xdg_surface_get_toplevel(toplevel->xdg_surface);
    if (!toplevel->toplevel)
        goto failed;
    xdg_toplevel_add_listener(toplevel->toplevel, &xdg_toplevel_listener, toplevel);
    wl_surface_commit(toplevel->surface);
    return 1;

failed:
    xdg_toplevel_client_destroy(toplevel);
    return 0;
}

int xdg_toplevel_client_complete_map(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel)
{
    if (!toplevel->xdg_surface || wl_display_roundtrip(connection->display) < 0 || !toplevel->configured)
        return 0;
    if (!xdg_toplevel_client_ack_latest_configure(connection, toplevel))
        return 0;
    return map_toplevel(connection, toplevel, 1, 1, 0xffffffffu);
}

int xdg_toplevel_client_create_with_surface_setup(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel,
    xdg_surface_setup_callback setup,
    void *data)
{
    return xdg_toplevel_client_create_internal(
        connection, toplevel, setup, data, 1, 1, 0xffffffffu);
}

int xdg_toplevel_client_create(struct client_connection *connection,
                                      struct xdg_toplevel_client *toplevel)
{
    return xdg_toplevel_client_create_with_surface_setup(connection, toplevel, NULL, NULL);
}

int xdg_toplevel_client_create_with_solid_buffer(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel,
    int width,
    int height,
    uint32_t argb)
{
    return xdg_toplevel_client_create_internal(
        connection, toplevel, NULL, NULL, width, height, argb);
}

void xdg_toplevel_client_destroy(struct xdg_toplevel_client *toplevel)
{
    if (toplevel->toplevel)
        xdg_toplevel_destroy(toplevel->toplevel);
    if (toplevel->xdg_surface)
        xdg_surface_destroy(toplevel->xdg_surface);
    if (toplevel->surface)
        wl_surface_destroy(toplevel->surface);
    if (toplevel->buffer)
        wl_buffer_destroy(toplevel->buffer);
    if (toplevel->shm)
        wl_shm_destroy(toplevel->shm);
    if (toplevel->wm_base)
        xdg_wm_base_destroy(toplevel->wm_base);
    if (toplevel->compositor)
        wl_compositor_destroy(toplevel->compositor);
    memset(toplevel, 0, sizeof(*toplevel));
}
