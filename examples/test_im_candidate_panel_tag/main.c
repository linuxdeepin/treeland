// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#define _GNU_SOURCE
#include "xdg-shell-client-protocol.h"
#include "xdg-toplevel-tag-v1-client-protocol.h"

#include <wayland-client.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static struct wl_display *display = NULL;
static struct wl_surface *surface = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *xdg_toplevel = NULL;
static struct wl_buffer *buffer = NULL;

static void cleanup(void)
{
    if (buffer)
        wl_buffer_destroy(buffer);
    if (xdg_toplevel)
        xdg_toplevel_destroy(xdg_toplevel);
    if (xdg_surface)
        xdg_surface_destroy(xdg_surface);
    if (surface)
        wl_surface_destroy(surface);
    if (display)
        wl_display_disconnect(display);
}

static struct wl_compositor *compositor = NULL;
static struct xdg_wm_base *wm_base = NULL;
static struct xdg_toplevel_tag_manager_v1 *tag_manager = NULL;
static struct wl_shm *shm = NULL;

static int create_shm_fd(int size)
{
    int fd = memfd_create("wayland-shm", MFD_CLOEXEC);
    if (fd < 0)
        return -1;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static struct wl_buffer *create_shm_buffer(int width, int height)
{
    int stride = width * 4;
    int size = stride * height;
    int fd = create_shm_fd(size);
    if (fd < 0) {
        fprintf(stderr, "Failed to create shm fd\n");
        return NULL;
    }

    uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    for (int i = 0; i < width * height; i++)
        data[i] = 0xFF2266AA;

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer =
        wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    return buffer;
}

static void xdg_surface_configure([[maybe_unused]] void *data,
                                  struct xdg_surface *xdg_surface,
                                  uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_wm_base_ping([[maybe_unused]] void *data,
                             struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void registry_global([[maybe_unused]] void *data,
                            struct wl_registry *registry,
                            uint32_t id,
                            const char *interface,
                            uint32_t version)
{
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, version);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, version);
        xdg_wm_base_add_listener(wm_base, &xdg_wm_base_listener, NULL);
    } else if (strcmp(interface, xdg_toplevel_tag_manager_v1_interface.name) == 0) {
        tag_manager = wl_registry_bind(registry, id, &xdg_toplevel_tag_manager_v1_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, version);
    }
}

static void registry_global_remove([[maybe_unused]] void *data,
                                   [[maybe_unused]] struct wl_registry *registry,
                                   [[maybe_unused]] uint32_t id)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main()
{
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !wm_base || !tag_manager || !shm) {
        fprintf(stderr, "Missing required interfaces\n");
        wl_display_disconnect(display);
        return 1;
    }

    int width = 400, height = 100;
    surface = wl_compositor_create_surface(compositor);

    xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_set_title(xdg_toplevel, "IM Candidate Panel Tag Test");
    xdg_toplevel_set_app_id(xdg_toplevel, "im-candidate-panel-tag");

    buffer = create_shm_buffer(width, height);
    if (!buffer) {
        fprintf(stderr, "Failed to create buffer\n");
        cleanup();
        return 1;
    }

    // Set tag BEFORE the first commit
    xdg_toplevel_tag_manager_v1_set_toplevel_tag(tag_manager,
                                                 xdg_toplevel,
                                                 "org.deepin.treeland.im-candidate-panel");
    fprintf(stderr, "Tag set: org.deepin.treeland.im-candidate-panel\n");

    // First commit: initial commit without buffer, wait for configure
    wl_surface_commit(surface);
    wl_display_roundtrip(display);

    // Attach buffer after configure is acked
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    while (wl_display_dispatch(display) != -1) { }

    cleanup();
    return 0;
}
