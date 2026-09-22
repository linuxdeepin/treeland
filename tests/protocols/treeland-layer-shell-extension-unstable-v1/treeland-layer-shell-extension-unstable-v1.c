// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "treeland-layer-shell-extension-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <stdio.h>

struct events { unsigned rejected; uint32_t reason; };
static void resizing(void *data, struct treeland_layer_shell_extension_object_v1 *object, uint32_t value)
{ (void)data; (void)object; (void)value; }
static void resize_rejected(void *data, struct treeland_layer_shell_extension_object_v1 *object,
                            uint32_t reason)
{ (void)object; struct events *events = data; events->rejected++; events->reason = reason; }
static const struct treeland_layer_shell_extension_object_v1_listener listener = { resizing, resize_rejected };

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name)) return 1;
    struct wl_compositor *compositor = client_bind(&connection, "wl_compositor", &wl_compositor_interface, 1);
    struct wl_output *output = client_bind(&connection, "wl_output", &wl_output_interface, 1);
    struct wl_seat *seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct zwlr_layer_shell_v1 *shell = client_bind(&connection, "zwlr_layer_shell_v1", &zwlr_layer_shell_v1_interface, 1);
    struct treeland_layer_shell_extension_manager_v1 *manager = client_bind(&connection,
        "treeland_layer_shell_extension_manager_v1", &treeland_layer_shell_extension_manager_v1_interface, 1);
    if (!compositor || !output || !seat || !shell || !manager) return 1;
    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct zwlr_layer_surface_v1 *layer = zwlr_layer_shell_v1_get_layer_surface(shell, surface, output,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP, "protocol-test");
    struct treeland_layer_shell_extension_object_v1 *object =
        treeland_layer_shell_extension_manager_v1_get_layer_shell_extension_object(manager, surface);
    struct events events = { 0 };
    if (!surface || !layer || !object) return 1;
    treeland_layer_shell_extension_object_v1_add_listener(object, &listener, &events);
    treeland_layer_shell_extension_object_v1_begin_resize(object, seat, 0,
        TREELAND_LAYER_SHELL_EXTENSION_OBJECT_V1_RESIZE_EDGE_BOTTOM, 0, 0, 0, 0);
    if (wl_display_roundtrip(connection.display) < 0 || events.rejected != 1
        || events.reason != TREELAND_LAYER_SHELL_EXTENSION_OBJECT_V1_RESIZE_ERROR_BAD_SERIAL) {
        fprintf(stderr, "layer-shell-extension: invalid resize was not rejected\n");
        return 1;
    }
    treeland_layer_shell_extension_object_v1_destroy(object);
    zwlr_layer_surface_v1_destroy(layer);
    wl_surface_destroy(surface);
    treeland_layer_shell_extension_manager_v1_destroy(manager);
    zwlr_layer_shell_v1_destroy(shell);
    wl_seat_destroy(seat); wl_output_destroy(output); wl_compositor_destroy(compositor);
    client_disconnect(&connection);
    return 0;
}
