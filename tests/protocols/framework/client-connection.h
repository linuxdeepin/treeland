// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

#define TEST_ERROR(...) fprintf(stderr, __VA_ARGS__)

#define TEST_READ_SERVER(callback, state, message) \
    (memset((state), 0, sizeof(*(state))), \
     invoke_on_server_thread((callback), (state)) ? 0 : (TEST_ERROR("%s\\n", (message)), 1))

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

struct client_pointer_seat {
    struct wl_seat *seat;
    struct wl_pointer *pointer;
};

struct client_seat {
    struct wl_seat *seat;
    uint32_t capabilities;
};

int client_connect(struct client_connection *connection, const char *socket_name);
void *client_bind(struct client_connection *connection, const char *interface,
                  const struct wl_interface *wl_interface, uint32_t version);
void *client_bind_global(struct client_connection *connection, uint32_t name,
                         const struct wl_interface *wl_interface, uint32_t version);
void *client_bind_last(struct client_connection *connection, const char *interface,
                       const struct wl_interface *wl_interface, uint32_t version);
int client_bind_seat(struct client_connection *connection, uint32_t version,
                     struct client_seat *seat);
void client_seat_destroy(struct client_seat *seat);
int client_bind_pointer_seat(struct client_connection *connection, uint32_t version,
                             struct client_pointer_seat *pointer_seat);
void client_pointer_seat_destroy(struct client_pointer_seat *pointer_seat);
void client_disconnect(struct client_connection *connection);

#ifdef __cplusplus
}
#endif
