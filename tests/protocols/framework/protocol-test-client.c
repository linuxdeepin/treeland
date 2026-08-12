/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Common registry and connection handling for pure-C protocol clients.
 */
#include "protocol-test-client.h"

#include <string.h>

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
{
    (void)registry;
    struct protocol_test_connection *connection = data;
    if (connection->global_count == 64)
        return;
    connection->globals[connection->global_count++] = (struct protocol_test_global) {
        .name = name,
        .version = version,
        .interface = interface,
    };
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

int protocol_test_connect(struct protocol_test_connection *connection, const char *socket_name)
{
    memset(connection, 0, sizeof(*connection));
    connection->display = wl_display_connect(socket_name);
    if (!connection->display)
        return 0;
    connection->registry = wl_display_get_registry(connection->display);
    wl_registry_add_listener(connection->registry, &registry_listener, connection);
    return wl_display_roundtrip(connection->display) >= 0;
}

void *protocol_test_bind(struct protocol_test_connection *connection, const char *interface,
                         const struct wl_interface *wl_interface, uint32_t version)
{
    for (uint32_t i = 0; i < connection->global_count; ++i) {
        const struct protocol_test_global *global = &connection->globals[i];
        if (strcmp(global->interface, interface) == 0)
            return wl_registry_bind(connection->registry, global->name, wl_interface,
                                    version < global->version ? version : global->version);
    }
    return NULL;
}

void protocol_test_disconnect(struct protocol_test_connection *connection)
{
    if (connection->registry)
        wl_registry_destroy(connection->registry);
    if (connection->display)
        wl_display_disconnect(connection->display);
    memset(connection, 0, sizeof(*connection));
}
