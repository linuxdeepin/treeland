// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-shell-client-protocol.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

/* Create a minimal 1×1 shm buffer so we can commit a wl_surface. */
static struct wl_buffer *create_shm_buffer(struct wl_shm *shm)
{
    if (!shm)
        return NULL;

    int fd = shm_open("/xdg-shell-test", O_CREAT | O_RDWR, 0600);
    if (fd < 0)
        return NULL;
    shm_unlink("/xdg-shell-test");

    if (ftruncate(fd, 4) < 0) {
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, 4);
    close(fd);
    if (!pool)
        return NULL;

    struct wl_buffer *buffer =
        wl_shm_pool_create_buffer(pool, 0, 1, 1, 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    return buffer;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    /* --- Positive: full toplevel lifecycle via the shared framework --- */
    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "xdg-shell: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    /* The framework must have received and acked the initial configure. */
    if (!tc.configured) {
        fprintf(stderr, "xdg-shell: initial configure was not received\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    xdg_toplevel_set_title(tc.toplevel, "test-title");
    xdg_toplevel_set_app_id(tc.toplevel, "test.app.id");
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-shell: roundtrip after set_title/app_id failed\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    xdg_toplevel_client_destroy(&tc);

    /* --- Negative: committing a buffer before acking the initial configure
     * must raise xdg_surface.error.unconfigured_buffer. */
    struct wl_compositor *compositor =
        client_bind(&conn, "wl_compositor", &wl_compositor_interface, 4);
    struct wl_shm *shm =
        client_bind(&conn, "wl_shm", &wl_shm_interface, 1);
    struct xdg_wm_base *wm_base =
        client_bind(&conn, "xdg_wm_base", &xdg_wm_base_interface, 1);
    if (!compositor || !shm || !wm_base) {
        fprintf(stderr, "xdg-shell: failed to bind required globals\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct xdg_surface *xdg_surface =
        xdg_wm_base_get_xdg_surface(wm_base, surface);
    (void)xdg_surface_get_toplevel(xdg_surface);

    struct wl_buffer *buffer = create_shm_buffer(shm);
    if (!buffer) {
        fprintf(stderr, "xdg-shell: failed to create shm buffer\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Commit a buffer WITHOUT acking the initial configure. */
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    if (wl_display_roundtrip(conn.display) >= 0) {
        fprintf(stderr, "xdg-shell: unconfigured buffer commit did not raise an error\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Expected protocol error raised; the connection is fatal. */
    return 0;
}
