// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>

#define TREELAND_SHELL_MANAGER_V1_VERSION 1

namespace Waylib::Server {
class WServer;
}

class ForeignToplevelManager;
class TreeLandHelper;
struct ztreeland_foreign_toplevel_manager_v1 {
    struct wl_global *global;

    struct {
        struct wl_signal destroy;
    } events;

    void *data;

    struct wl_list contexts;  // link to treeland_socket_context_v1.link

    struct wl_listener display_destroy;

    ForeignToplevelManager *manager = nullptr;
};

struct ztreeland_foreign_toplevel_handle_v1 {
    struct ztreeland_foreign_toplevel_manager_v1      *manager;
    struct wl_list link;  // treeland_shell_manager_v1.contexts
};

struct ztreeland_foreign_toplevel_manager_v1 *
foreign_toplevel_manager_from_resource(struct wl_resource *resource);

void foreign_toplevel_manager_bind(struct wl_client *client,
                                   void             *data,
                                   uint32_t          version,
                                   uint32_t          id);

void foreign_toplevel_manager_handle_display_destroy(
    struct wl_listener *listener, void *data);
