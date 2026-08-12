/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * C ABI between a pure-C Wayland client and the C++ protocol-test fixture.
 */
#pragma once

#include <stdint.h>
#include <wayland-client.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*protocol_test_server_callback)(void *data);

struct protocol_test_global {
    uint32_t name;
    uint32_t version;
    const char *interface;
};

struct protocol_test_connection {
    struct wl_display *display;
    struct wl_registry *registry;
    struct protocol_test_global globals[64];
    uint32_t global_count;
};

int protocol_test_connect(struct protocol_test_connection *connection, const char *socket_name);
void *protocol_test_bind(struct protocol_test_connection *connection, const char *interface,
                         const struct wl_interface *wl_interface, uint32_t version);
void protocol_test_disconnect(struct protocol_test_connection *connection);

/* Runs callback on the compositor's Qt thread and waits for it to finish. */
int protocol_test_invoke_server(protocol_test_server_callback callback, void *data);

/* Implemented by each protocol's pure-C client test. Returns zero on success. */
int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
