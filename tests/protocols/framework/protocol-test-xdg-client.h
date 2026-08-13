/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 * Reusable scanner-generated xdg-shell client support for protocol tests.
 */
#pragma once

#include "protocol-test-client.h"
#include "xdg-shell-client-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct protocol_test_xdg_toplevel {
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

typedef int (*protocol_test_xdg_surface_setup)(struct wl_surface *surface, void *data);

/* Creates an xdg_toplevel and commits its initial empty state. It deliberately
 * does not wait for xdg_surface.configure: protocols such as app-id-resolver
 * may defer wrapper creation, and therefore configure, until a separate
 * protocol response arrives. */
int protocol_test_xdg_toplevel_create_pending(
    struct protocol_test_connection *connection,
    struct protocol_test_xdg_toplevel *toplevel);
/* Waits for the configure emitted after a pending toplevel becomes eligible
 * for mapping, acknowledges it, then attaches a 1x1 wl_shm buffer. */
int protocol_test_xdg_toplevel_complete_map(
    struct protocol_test_connection *connection,
    struct protocol_test_xdg_toplevel *toplevel);

/* Creates a configured xdg_toplevel and maps it by attaching a 1x1 wl_shm
 * buffer. The fixture must advertise wl_shm before the client connects. */
int protocol_test_xdg_toplevel_create(struct protocol_test_connection *connection,
                                      struct protocol_test_xdg_toplevel *toplevel);
/* Creates a configured xdg_toplevel and maps it with a solid ARGB8888 wl_shm
 * buffer.  This is the client-side input for rendered-output integration
 * tests: the server must observe the colour after it enters the real scene. */
int protocol_test_xdg_toplevel_create_with_solid_buffer(
    struct protocol_test_connection *connection,
    struct protocol_test_xdg_toplevel *toplevel,
    int width,
    int height,
    uint32_t argb);
/* Invokes setup after wl_surface creation but before assigning the xdg role.
 * Protocols such as DDE shell use this point to associate metadata that
 * ShellHandler consumes while constructing the SurfaceWrapper. */
int protocol_test_xdg_toplevel_create_with_surface_setup(
    struct protocol_test_connection *connection,
    struct protocol_test_xdg_toplevel *toplevel,
    protocol_test_xdg_surface_setup setup,
    void *data);
/* Acknowledge the latest configure received by the toplevel, if any. */
int protocol_test_xdg_toplevel_ack_latest_configure(
    struct protocol_test_connection *connection,
    struct protocol_test_xdg_toplevel *toplevel);
void protocol_test_xdg_toplevel_destroy(struct protocol_test_xdg_toplevel *toplevel);

#ifdef __cplusplus
}
#endif
