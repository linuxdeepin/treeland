// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>

#include <QString>

#define TREELAND_SHELL_MANAGER_V1_VERSION 1

namespace Waylib::Server {
class WServer;
}

class ForeignToplevelManager;
class TreeLandHelper;

struct ztreeland_foreign_toplevel_manager_v1
{
    struct wl_global *global;

    struct
    {
        struct wl_signal handleCreated;
        struct wl_signal destroy;
    } events;

    void *data;

    struct wl_list contexts;
    struct wl_event_loop *event_loop;

    struct wl_listener display_destroy;

    std::vector<struct wl_resource *> clients;
    std::vector<struct wl_resource *> surfaces;
};

struct ztreeland_foreign_toplevel_handle_v1
{
    struct ztreeland_foreign_toplevel_manager_v1 *manager;
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_resource *surface;
};

struct ztreeland_foreign_toplevel_manager_v1 *
foreign_toplevel_manager_from_resource(struct wl_resource *resource);

void foreign_toplevel_manager_bind(struct wl_client *client,
                                   void *data,
                                   uint32_t version,
                                   uint32_t id);

void foreign_toplevel_manager_handle_display_destroy(struct wl_listener *listener, void *data);

struct ztreeland_foreign_toplevel_manager_v1 *
ztreeland_foreign_toplevel_manager_v1_create(struct wl_display *display);
void ztreeland_foreign_toplevel_manager_v1_destroy(
    struct ztreeland_foreign_toplevel_manager_v1 *handle);
void ztreeland_foreign_toplevel_manager_v1_toplevel(
    struct ztreeland_foreign_toplevel_manager_v1 *handle, struct wl_resource *resource);

void ztreeland_foreign_toplevel_handle_v1_closed(
    struct ztreeland_foreign_toplevel_manager_v1 *handle, struct wl_resource *resource);

void ztreeland_foreign_toplevel_handle_v1_done(struct ztreeland_foreign_toplevel_manager_v1 *handle,
                                               struct wl_resource *resource);
void ztreeland_foreign_toplevel_handle_v1_pid(struct ztreeland_foreign_toplevel_manager_v1 *handle,
                                              struct wl_resource *resource,
                                              uint32_t pid);
void ztreeland_foreign_toplevel_handle_v1_identifier(
    struct ztreeland_foreign_toplevel_manager_v1 *handle,
    struct wl_resource *resource,
    const QString &identifier);