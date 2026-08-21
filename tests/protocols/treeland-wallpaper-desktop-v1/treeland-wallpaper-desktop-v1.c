// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-wallpaper-desktop-v1.h"
#include "treeland-wallpaper-manager-unstable-v1-client-protocol.h"
#include "treeland-wallpaper-shell-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void wallpaper_desktop_read_state(void *data);

static const char wallpaper_source[] = "/tmp/treeland-protocol-wallpaper-red";

struct wallpaper_client {
    struct client_connection connection;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_output *output;
    struct treeland_wallpaper_manager_v1 *manager;
    struct treeland_wallpaper_shell_v1 *shell;
    struct treeland_wallpaper_v1 *wallpaper;
    struct treeland_wallpaper_surface_v1 *wallpaper_surface;
    struct wl_surface *surface;
    struct wl_buffer *buffer;
    void *buffer_data;
    size_t buffer_size;
};

static int create_red_buffer(struct wallpaper_client *client)
{
    enum { width = 64, height = 64, stride = width * 4 };
    const size_t size = (size_t)stride * height;
    char name[64];
    snprintf(name, sizeof(name), "/treeland_wallpaper_%d", (int)getpid());
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return 0;
    }
    client->buffer_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (client->buffer_data == MAP_FAILED) {
        client->buffer_data = NULL;
        close(fd);
        return 0;
    }
    for (size_t i = 0; i < size / sizeof(uint32_t); ++i)
        ((uint32_t *)client->buffer_data)[i] = 0xffff0000u;

    struct wl_shm_pool *pool = wl_shm_create_pool(client->shm, fd, (int)size);
    close(fd);
    if (!pool)
        return 0;
    client->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                               WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!client->buffer)
        return 0;
    client->buffer_size = size;
    return 1;
}

static void cleanup(struct wallpaper_client *client)
{
    if (client->wallpaper_surface)
        treeland_wallpaper_surface_v1_destroy(client->wallpaper_surface);
    if (client->wallpaper)
        treeland_wallpaper_v1_destroy(client->wallpaper);
    if (client->shell)
        treeland_wallpaper_shell_v1_destroy(client->shell);
    if (client->manager)
        treeland_wallpaper_manager_v1_destroy(client->manager);
    if (client->buffer)
        wl_buffer_destroy(client->buffer);
    if (client->surface)
        wl_surface_destroy(client->surface);
    if (client->buffer_data)
        munmap(client->buffer_data, client->buffer_size);
    client_disconnect(&client->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct wallpaper_client client = { 0 };
    struct wallpaper_desktop_state state = { 0 };
    int result = 1;
    const char *stage = "connect";

    if (!client_connect(&client.connection, socket_name))
        goto done;
    stage = "bind";
    client.compositor = client_bind(&client.connection, "wl_compositor",
                                           &wl_compositor_interface, 1);
    client.shm = client_bind(&client.connection, "wl_shm", &wl_shm_interface, 1);
    client.output = client_bind(&client.connection, "wl_output", &wl_output_interface, 1);
    client.manager = client_bind(&client.connection, "treeland_wallpaper_manager_v1",
                                        &treeland_wallpaper_manager_v1_interface, 1);
    client.shell = client_bind(&client.connection, "treeland_wallpaper_shell_v1",
                                      &treeland_wallpaper_shell_v1_interface, 2);
    if (!client.compositor || !client.shm || !client.output || !client.manager || !client.shell)
        goto done;

    stage = "create-surface-buffer";
    client.surface = wl_compositor_create_surface(client.compositor);
    if (!client.surface || !create_red_buffer(&client))
        goto done;

    stage = "manager-wallpaper";
    client.wallpaper = treeland_wallpaper_manager_v1_get_treeland_wallpaper(
        client.manager, client.output, client.surface);
    if (!client.wallpaper)
        goto done;
    treeland_wallpaper_v1_set_image_source(
        client.wallpaper, wallpaper_source, TREELAND_WALLPAPER_V1_WALLPAPER_ROLE_DESKTOP);

    stage = "shell-wallpaper";
    client.wallpaper_surface = treeland_wallpaper_shell_v1_get_treeland_wallpaper_surface(
        client.shell, client.surface, wallpaper_source);
    if (!client.wallpaper_surface)
        goto done;
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    // wl_compositor is intentionally bound at v1, so the wl_surface is also
    // v1. damage_buffer is since v4; use the v1 damage request instead.
    wl_surface_damage(client.surface, 0, 0, 64, 64);
    wl_surface_commit(client.surface);
    treeland_wallpaper_surface_v1_ready(client.wallpaper_surface);

    // Wayland roundtrip is the ordering barrier for manager configuration and
    // wallpaper-shell registration. Mapping remains the responsibility of the
    // production wallpaper owner, not this role-less client surface.
    stage = "roundtrip-or-server-state";
    if (wl_display_roundtrip(client.connection.display) < 0
        || !invoke_on_server_thread(wallpaper_desktop_read_state, &state))
        goto done;

    stage = "production-wallpaper-result";
    if (state.shell_surface_registered && state.manager_reference_matched
        && state.output_matched && state.manager_configured)
        result = 0;

done:
    if (result != 0) {
        fprintf(stderr,
                "wallpaper desktop failed at %s: shell=(registered=%d) "
                "manager=(reference=%d output=%d configured=%d) "
                "\n",
                stage,
                state.shell_surface_registered,
                state.manager_reference_matched,
                state.output_matched,
                state.manager_configured);
    }
    cleanup(&client);
    return result;
}
