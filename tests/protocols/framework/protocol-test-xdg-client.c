/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Reusable scanner-generated xdg-shell client support for protocol tests.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "protocol-test-xdg-client.h"

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
    struct protocol_test_xdg_toplevel *toplevel = data;
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
    ((struct protocol_test_xdg_toplevel *)data)->close_received = 1;
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

static int map_toplevel(struct protocol_test_connection *connection,
                        struct protocol_test_xdg_toplevel *toplevel)
{
    const int stride = 4;
    char name[64];
    snprintf(name, sizeof(name), "/treeland_protocol_xdg_%d", (int)getpid());
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    if (ftruncate(fd, stride) < 0) {
        close(fd);
        return 0;
    }
    void *data = mmap(NULL, stride, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return 0;
    }
    memset(data, 0xff, stride);
    struct wl_shm_pool *pool = wl_shm_create_pool(toplevel->shm, fd, stride);
    toplevel->buffer = pool ? wl_shm_pool_create_buffer(pool, 0, 1, 1, stride,
                                                         WL_SHM_FORMAT_ARGB8888) : NULL;
    if (pool)
        wl_shm_pool_destroy(pool);
    munmap(data, stride);
    close(fd);
    if (!toplevel->buffer)
        return 0;
    wl_surface_attach(toplevel->surface, toplevel->buffer, 0, 0);
    wl_surface_damage(toplevel->surface, 0, 0, 1, 1);
    wl_surface_commit(toplevel->surface);
    return wl_display_roundtrip(connection->display) >= 0;
}

int protocol_test_xdg_toplevel_create_with_surface_setup(
    struct protocol_test_connection *connection,
    struct protocol_test_xdg_toplevel *toplevel,
    protocol_test_xdg_surface_setup setup,
    void *data)
{
    memset(toplevel, 0, sizeof(*toplevel));
    toplevel->compositor = protocol_test_bind(connection, "wl_compositor", &wl_compositor_interface, 1);
    toplevel->wm_base = protocol_test_bind(connection, "xdg_wm_base", &xdg_wm_base_interface, 1);
    toplevel->shm = protocol_test_bind(connection, "wl_shm", &wl_shm_interface, 1);
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
    xdg_surface_ack_configure(toplevel->xdg_surface, toplevel->configure_serial);
    return map_toplevel(connection, toplevel);

failed:
    protocol_test_xdg_toplevel_destroy(toplevel);
    return 0;
}

int protocol_test_xdg_toplevel_create(struct protocol_test_connection *connection,
                                      struct protocol_test_xdg_toplevel *toplevel)
{
    return protocol_test_xdg_toplevel_create_with_surface_setup(connection, toplevel, NULL, NULL);
}

void protocol_test_xdg_toplevel_destroy(struct protocol_test_xdg_toplevel *toplevel)
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
