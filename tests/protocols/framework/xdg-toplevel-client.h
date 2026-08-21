// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "client-connection.h"
#include "xdg-shell-client-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xdg_toplevel_client {
    struct wl_compositor *compositor;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    struct wl_shm *shm;
    struct wl_buffer *buffer;
    uint32_t configure_serial;
    uint32_t acknowledged_configure_serial;
    int configured;
    int close_received;
};

typedef int (*xdg_surface_setup_callback)(struct wl_surface *surface, void *data);

int xdg_toplevel_client_create_pending(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel);

int xdg_toplevel_client_complete_map(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel);
int xdg_toplevel_client_create(struct client_connection *connection,
                               struct xdg_toplevel_client *toplevel);

int xdg_toplevel_client_create_with_solid_buffer(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel,
    int width,
    int height,
    uint32_t argb);

int xdg_toplevel_client_create_with_surface_setup(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel,
    xdg_surface_setup_callback setup,
    void *data);

int xdg_toplevel_client_ack_latest_configure(
    struct client_connection *connection,
    struct xdg_toplevel_client *toplevel);
void xdg_toplevel_client_destroy(struct xdg_toplevel_client *toplevel);

#ifdef __cplusplus
}
#endif
