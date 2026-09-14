// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"

#include <string.h>

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
{
    (void)registry;
    struct client_connection *connection = data;
    if (connection->global_count == CLIENT_CONNECTION_MAX_GLOBALS)
        return;
    struct client_global *global = &connection->globals[connection->global_count++];
    global->name = name;
    global->version = version;
    strncpy(global->interface, interface, sizeof(global->interface) - 1);
    global->interface[sizeof(global->interface) - 1] = '\0';
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

int client_connect(struct client_connection *connection, const char *socket_name)
{
    memset(connection, 0, sizeof(*connection));
    connection->display = wl_display_connect(socket_name);
    if (!connection->display)
        return 0;
    connection->registry = wl_display_get_registry(connection->display);
    wl_registry_add_listener(connection->registry, &registry_listener, connection);
    return wl_display_roundtrip(connection->display) >= 0;
}

void *client_bind(struct client_connection *connection, const char *interface,
                  const struct wl_interface *wl_interface, uint32_t version)
{
    for (uint32_t i = 0; i < connection->global_count; ++i) {
        const struct client_global *global = &connection->globals[i];
        if (strcmp(global->interface, interface) == 0)
            return wl_registry_bind(connection->registry, global->name, wl_interface,
                                    version < global->version ? version : global->version);
    }
    return NULL;
}

void *client_bind_global(struct client_connection *connection, uint32_t name,
                         const struct wl_interface *wl_interface, uint32_t version)
{
    for (uint32_t i = 0; i < connection->global_count; ++i) {
        const struct client_global *global = &connection->globals[i];
        if (global->name == name && strcmp(global->interface, wl_interface->name) == 0)
            return wl_registry_bind(connection->registry, global->name, wl_interface,
                                    version < global->version ? version : global->version);
    }
    return NULL;
}

void *client_bind_last(struct client_connection *connection, const char *interface,
                       const struct wl_interface *wl_interface, uint32_t version)
{
    for (uint32_t i = connection->global_count; i > 0; --i) {
        const struct client_global *global = &connection->globals[i - 1];
        if (strcmp(global->interface, interface) == 0)
            return wl_registry_bind(connection->registry, global->name, wl_interface,
                                    version < global->version ? version : global->version);
    }
    return NULL;
}

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    (void)seat;
    *(uint32_t *)data = capabilities;
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

int client_bind_seat(struct client_connection *connection, uint32_t version,
                     struct client_seat *seat)
{
    seat->seat = client_bind(connection, wl_seat_interface.name, &wl_seat_interface, version);
    if (!seat->seat)
        return 0;
    wl_seat_add_listener(seat->seat, &seat_listener, &seat->capabilities);
    if (wl_display_roundtrip(connection->display) < 0) {
        client_seat_destroy(seat);
        return 0;
    }
    return 1;
}

void client_seat_destroy(struct client_seat *seat)
{
    if (seat->seat) {
        wl_seat_destroy(seat->seat);
        seat->seat = NULL;
    }
    seat->capabilities = 0;
}

int client_bind_pointer_seat(struct client_connection *connection, uint32_t version,
                             struct client_pointer_seat *pointer_seat)
{
    struct client_seat seat = { 0 };
    if (!client_bind_seat(connection, version, &seat))
        return 0;
    if (!(seat.capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        client_seat_destroy(&seat);
        return 0;
    }
    pointer_seat->seat = seat.seat;
    pointer_seat->pointer = wl_seat_get_pointer(pointer_seat->seat);
    if (!pointer_seat->pointer) {
        client_pointer_seat_destroy(pointer_seat);
        return 0;
    }
    return 1;
}

void client_pointer_seat_destroy(struct client_pointer_seat *pointer_seat)
{
    if (pointer_seat->pointer) {
        wl_pointer_destroy(pointer_seat->pointer);
        pointer_seat->pointer = NULL;
    }
    if (pointer_seat->seat) {
        wl_seat_destroy(pointer_seat->seat);
        pointer_seat->seat = NULL;
    }
}

void client_disconnect(struct client_connection *connection)
{
    if (connection->registry)
        wl_registry_destroy(connection->registry);
    if (connection->display)
        wl_display_disconnect(connection->display);
    memset(connection, 0, sizeof(*connection));
}
