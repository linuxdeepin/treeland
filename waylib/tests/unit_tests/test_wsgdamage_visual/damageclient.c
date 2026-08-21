// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#define _GNU_SOURCE

#include "damageclient.h"
#include "xdg-shell-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

enum {
    CMD_DAMAGE = 1,
    CMD_DAMAGE_ONLY = 2,
    CMD_STOP = 3,
    kBufferCount = 2
};

static const uint32_t kBasePixel = 0xFF2A8F4E;
static const uint32_t kDamagePixel = 0xFFE8C44A;

struct DamageCmd {
    int op;
    int x;
    int y;
    int w;
    int h;
};

struct ShmBuffer {
    struct wl_buffer *wl;
    uint32_t *pixels;
    size_t size;
    int busy;
};

struct DamageClient {
    int connect_fd;
    int cmd_rd;
    int cmd_wr;
    int ack_rd;
    int ack_wr;
    atomic_int mapped;
    atomic_int running;
    int width;
    int height;
    int configured;
    char error[128];

    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct ShmBuffer buffers[kBufferCount];
    struct ShmBuffer *current_buffer;
};

static void set_error(struct DamageClient *c, const char *msg)
{
    snprintf(c->error, sizeof(c->error), "%s", msg);
}

static int write_full(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    size_t left = n;
    while (left) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += (size_t)w;
        left -= (size_t)w;
    }
    return 0;
}

static int read_full(int fd, void *buf, size_t n)
{
    char *p = buf;
    size_t left = n;
    while (left) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return -1;
        p += (size_t)r;
        left -= (size_t)r;
    }
    return 0;
}

static int create_shm_fd(size_t size)
{
    int fd = memfd_create("wsg-damage-client", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
        return -1;
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    (void)buffer;
    struct ShmBuffer *b = data;
    b->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static int create_shm_buffer(struct DamageClient *c, struct ShmBuffer *out)
{
    const int stride = c->width * 4;
    const size_t size = (size_t)stride * (size_t)c->height;
    int fd = create_shm_fd(size);
    if (fd < 0)
        return -1;

    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        close(fd);
        return -1;
    }

    for (int i = 0; i < c->width * c->height; ++i)
        pixels[i] = (uint32_t)kBasePixel;

    struct wl_shm_pool *pool = wl_shm_create_pool(c->shm, fd, (int32_t)size);
    close(fd);
    if (!pool) {
        munmap(pixels, size);
        return -1;
    }

    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, c->width, c->height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    if (!buffer) {
        munmap(pixels, size);
        return -1;
    }

    out->wl = buffer;
    out->pixels = pixels;
    out->size = size;
    out->busy = 0;
    wl_buffer_add_listener(buffer, &buffer_listener, out);
    return 0;
}

static struct ShmBuffer *pick_buffer(struct DamageClient *c)
{
    for (int i = 0; i < kBufferCount; ++i) {
        if (!c->buffers[i].busy)
            return &c->buffers[i];
    }
    return &c->buffers[0];
}

static void fill_rect(struct ShmBuffer *b, int width, int x, int y, int w, int h, uint32_t pixel)
{
    for (int row = y; row < y + h; ++row) {
        uint32_t *line = b->pixels + row * width + x;
        for (int col = 0; col < w; ++col)
            line[col] = pixel;
    }
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct DamageClient *c = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    c->configured = 1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                   int32_t width, int32_t height, struct wl_array *states)
{
    (void)toplevel;
    (void)states;
    struct DamageClient *c = data;
    if (width > 0)
        c->width = width;
    if (height > 0)
        c->height = height;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    (void)data;
    (void)toplevel;
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

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
{
    struct DamageClient *c = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        const uint32_t v = version < 4 ? version : 4;
        c->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, v);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        c->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        const uint32_t v = version < 5 ? version : 5;
        c->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, v);
        xdg_wm_base_add_listener(c->wm_base, &wm_base_listener, c);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void destroy_wayland(struct DamageClient *c)
{
    for (int i = 0; i < kBufferCount; ++i) {
        if (c->buffers[i].wl) {
            wl_buffer_destroy(c->buffers[i].wl);
            c->buffers[i].wl = NULL;
        }
        if (c->buffers[i].pixels) {
            munmap(c->buffers[i].pixels, c->buffers[i].size);
            c->buffers[i].pixels = NULL;
        }
    }
    if (c->xdg_toplevel) {
        xdg_toplevel_destroy(c->xdg_toplevel);
        c->xdg_toplevel = NULL;
    }
    if (c->xdg_surface) {
        xdg_surface_destroy(c->xdg_surface);
        c->xdg_surface = NULL;
    }
    if (c->surface) {
        wl_surface_destroy(c->surface);
        c->surface = NULL;
    }
    if (c->wm_base) {
        xdg_wm_base_destroy(c->wm_base);
        c->wm_base = NULL;
    }
    if (c->shm) {
        wl_shm_destroy(c->shm);
        c->shm = NULL;
    }
    if (c->compositor) {
        wl_compositor_destroy(c->compositor);
        c->compositor = NULL;
    }
    if (c->registry) {
        wl_registry_destroy(c->registry);
        c->registry = NULL;
    }
    if (c->display) {
        wl_display_disconnect(c->display);
        c->display = NULL;
    }
}

static int commit_damage(struct DamageClient *c, int x, int y, int w, int h, int attach_buffer)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > c->width || y + h > c->height)
        return -1;

    struct ShmBuffer *buf = attach_buffer ? pick_buffer(c) : c->current_buffer;
    if (!buf)
        return -1;
    for (int i = 0; i < c->width * c->height; ++i)
        buf->pixels[i] = (uint32_t)kBasePixel;
    fill_rect(buf, c->width, x, y, w, h, (uint32_t)kDamagePixel);
    if (attach_buffer) {
        buf->busy = 1;
        c->current_buffer = buf;
        wl_surface_attach(c->surface, buf->wl, 0, 0);
    }
    wl_surface_damage(c->surface, x, y, w, h);
    wl_surface_commit(c->surface);
    return wl_display_flush(c->display) < 0 ? -1 : 0;
}

struct DamageClient *damage_client_create(int fd)
{
    if (fd < 0)
        return NULL;

    struct DamageClient *c = calloc(1, sizeof(*c));
    if (!c) {
        close(fd);
        return NULL;
    }

    c->connect_fd = fd;
    c->width = DAMAGE_CLIENT_WIDTH;
    c->height = DAMAGE_CLIENT_HEIGHT;
    c->cmd_rd = c->cmd_wr = c->ack_rd = c->ack_wr = -1;

    int cmd[2] = { -1, -1 };
    int ack[2] = { -1, -1 };
    if (pipe(cmd) < 0 || pipe(ack) < 0) {
        if (cmd[0] >= 0)
            close(cmd[0]);
        if (cmd[1] >= 0)
            close(cmd[1]);
        if (ack[0] >= 0)
            close(ack[0]);
        if (ack[1] >= 0)
            close(ack[1]);
        close(fd);
        free(c);
        return NULL;
    }
    c->cmd_rd = cmd[0];
    c->cmd_wr = cmd[1];
    c->ack_rd = ack[0];
    c->ack_wr = ack[1];
    atomic_store(&c->running, 1);
    return c;
}

void damage_client_run(struct DamageClient *c)
{
    if (!c)
        return;

    c->display = wl_display_connect_to_fd(c->connect_fd);
    c->connect_fd = -1;
    if (!c->display) {
        set_error(c, "wl_display_connect_to_fd failed");
        atomic_store(&c->running, 0);
        return;
    }

    c->registry = wl_display_get_registry(c->display);
    wl_registry_add_listener(c->registry, &registry_listener, c);
    if (wl_display_roundtrip(c->display) < 0) {
        set_error(c, "registry roundtrip failed");
        goto out;
    }
    if (!c->compositor || !c->shm || !c->wm_base) {
        set_error(c, "missing wl_compositor, wl_shm, or xdg_wm_base");
        goto out;
    }

    c->surface = wl_compositor_create_surface(c->compositor);
    c->xdg_surface = xdg_wm_base_get_xdg_surface(c->wm_base, c->surface);
    xdg_surface_add_listener(c->xdg_surface, &xdg_surface_listener, c);
    c->xdg_toplevel = xdg_surface_get_toplevel(c->xdg_surface);
    xdg_toplevel_add_listener(c->xdg_toplevel, &xdg_toplevel_listener, c);
    xdg_toplevel_set_title(c->xdg_toplevel, "wsg-damage-client");
    wl_surface_commit(c->surface);
    if (wl_display_roundtrip(c->display) < 0) {
        set_error(c, "xdg configure roundtrip failed");
        goto out;
    }
    if (!c->configured) {
        set_error(c, "xdg configure not received");
        goto out;
    }

    for (int i = 0; i < kBufferCount; ++i) {
        if (create_shm_buffer(c, &c->buffers[i]) < 0) {
            set_error(c, "shm buffer create failed");
            goto out;
        }
    }

    struct ShmBuffer *buf = pick_buffer(c);
    buf->busy = 1;
    c->current_buffer = buf;
    wl_surface_attach(c->surface, buf->wl, 0, 0);
    wl_surface_damage(c->surface, 0, 0, c->width, c->height);
    wl_surface_commit(c->surface);
    if (wl_display_flush(c->display) < 0) {
        set_error(c, "map flush failed");
        goto out;
    }
    atomic_store(&c->mapped, 1);

    while (atomic_load(&c->running)) {
        struct DamageCmd cmd;
        if (read_full(c->cmd_rd, &cmd, sizeof(cmd)) < 0)
            break;
        if (cmd.op == CMD_STOP)
            break;
        if (cmd.op == CMD_DAMAGE || cmd.op == CMD_DAMAGE_ONLY) {
            int status = commit_damage(c, cmd.x, cmd.y, cmd.w, cmd.h,
                                       cmd.op == CMD_DAMAGE);
            write_full(c->ack_wr, &status, sizeof(status));
            wl_display_dispatch_pending(c->display);
        }
    }

out:
    atomic_store(&c->mapped, 0);
    atomic_store(&c->running, 0);
    destroy_wayland(c);
}

int damage_client_is_mapped(const struct DamageClient *client)
{
    return client && atomic_load(&client->mapped);
}

const char *damage_client_error(const struct DamageClient *client)
{
    if (!client)
        return "null client";
    return client->error[0] ? client->error : "";
}

static int commit_damage_command(struct DamageClient *client, int op,
                                 int x, int y, int w, int h)
{
    if (!client || !atomic_load(&client->mapped))
        return -1;

    struct DamageCmd cmd = {
        .op = op,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    if (write_full(client->cmd_wr, &cmd, sizeof(cmd)) < 0)
        return -1;

    int status = -1;
    if (read_full(client->ack_rd, &status, sizeof(status)) < 0)
        return -1;
    return status;
}

int damage_client_commit_damage(struct DamageClient *client, int x, int y, int w, int h)
{
    return commit_damage_command(client, CMD_DAMAGE, x, y, w, h);
}

int damage_client_commit_damage_only(struct DamageClient *client, int x, int y, int w, int h)
{
    return commit_damage_command(client, CMD_DAMAGE_ONLY, x, y, w, h);
}

void damage_client_stop(struct DamageClient *client)
{
    if (!client)
        return;
    if (atomic_exchange(&client->running, 0)) {
        struct DamageCmd cmd = { .op = CMD_STOP };
        write_full(client->cmd_wr, &cmd, sizeof(cmd));
    }
}

void damage_client_destroy(struct DamageClient *client)
{
    if (!client)
        return;
    damage_client_stop(client);
    if (client->cmd_rd >= 0)
        close(client->cmd_rd);
    if (client->cmd_wr >= 0)
        close(client->cmd_wr);
    if (client->ack_rd >= 0)
        close(client->ack_rd);
    if (client->ack_wr >= 0)
        close(client->ack_wr);
    if (client->connect_fd >= 0)
        close(client->connect_fd);
    free(client);
}
