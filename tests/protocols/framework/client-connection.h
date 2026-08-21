// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <stdint.h>
#include <wayland-client.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { CLIENT_CONNECTION_MAX_GLOBALS = 256 };
enum { CLIENT_CONNECTION_MAX_INTERFACE_NAME = 128 };

struct client_global {
    uint32_t name;
    uint32_t version;
    char interface[CLIENT_CONNECTION_MAX_INTERFACE_NAME];
};

struct client_connection {
    struct wl_display *display;
    struct wl_registry *registry;
    struct client_global globals[CLIENT_CONNECTION_MAX_GLOBALS];
    uint32_t global_count;
};

int client_connect(struct client_connection *connection, const char *socket_name);
void *client_bind(struct client_connection *connection, const char *interface,
                  const struct wl_interface *wl_interface, uint32_t version);
void client_disconnect(struct client_connection *connection);

#ifdef __cplusplus
}
#endif
