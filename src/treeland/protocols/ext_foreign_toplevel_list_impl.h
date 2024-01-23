// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wtoplevelsurface.h"

#include <wayland-server-core.h>
#include <wayland-server.h>

class ExtForeignToplevelList;
class Helper;
struct ext_foreign_toplevel_handle_v1;

struct ext_foreign_toplevel_list_v1
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

struct ext_foreign_toplevel_handle_v1
{
    struct ext_foreign_toplevel_list_v1 *manager;
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_resource *surface;
};

struct ext_foreign_toplevel_list_v1 *
foreign_toplevel_list_from_resource(struct wl_resource *resource);
void ext_foreign_toplevel_list_bind(struct wl_client *client,
                                    void *data,
                                    uint32_t version,
                                    uint32_t id);
void ext_foreign_toplevel_list_handle_display_destroy(struct wl_listener *listener, void *data);

struct ext_foreign_toplevel_list_v1 *
ext_foreign_toplevel_list_v1_create(struct wl_display *display);
void ext_foreign_toplevel_list_v1_destroy(struct ext_foreign_toplevel_list_v1 *handle);
void ext_foreign_toplevel_list_v1_toplevel(struct ext_foreign_toplevel_list_v1 *handle,
                                           struct wl_resource *resource);

void ext_foreign_toplevel_handle_v1_closed(struct ext_foreign_toplevel_list_v1 *handle,
                                           struct wl_resource *resource);

void ext_foreign_toplevel_handle_v1_done(struct ext_foreign_toplevel_list_v1 *handle,
                                         struct wl_resource *resource);
void ext_foreign_toplevel_handle_v1_title(struct ext_foreign_toplevel_list_v1 *handle,
                                          struct wl_resource *resource,
                                          const QString &title);
void ext_foreign_toplevel_handle_v1_app_id(struct ext_foreign_toplevel_list_v1 *handle,
                                           struct wl_resource *resource,
                                           const QString &appId);
void ext_foreign_toplevel_handle_v1_identifier(struct ext_foreign_toplevel_list_v1 *handle,
                                               struct wl_resource *resource,
                                               const QString &identifier);
