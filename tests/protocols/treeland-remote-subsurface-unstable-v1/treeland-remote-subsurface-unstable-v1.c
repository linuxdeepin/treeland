// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-remote-subsurface-unstable-v1.h"
#include "xdg-toplevel-client.h"
#include "treeland-remote-subsurface-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct export_state {
    char token[129];
    unsigned parent_rejected;
    uint32_t rejected_reason;
    char rejected_token[129];
};

static void surface_token(void *data, struct treeland_exported_surface_v1 *exported,
                          const char *token)
{
    (void)exported;
    struct export_state *state = data;
    snprintf(state->token, sizeof(state->token), "%s", token);
}

static void parent_rejected(void *data, struct treeland_exported_surface_v1 *exported,
                            uint32_t reason, const char *token)
{
    (void)exported;
    struct export_state *state = data;
    state->parent_rejected++;
    state->rejected_reason = reason;
    snprintf(state->rejected_token, sizeof(state->rejected_token), "%s", token);
}

static const struct treeland_exported_surface_v1_listener exported_listener = {
    surface_token, parent_rejected,
};

struct remote_state {
    unsigned invalid_sibling;
    char sibling_token[129];
};

static void invalid_sibling(void *data, struct treeland_remote_subsurface_v1 *remote,
                            const char *token)
{
    (void)remote;
    struct remote_state *state = data;
    state->invalid_sibling++;
    snprintf(state->sibling_token, sizeof(state->sibling_token), "%s", token);
}

static const struct treeland_remote_subsurface_v1_listener remote_listener = {
    invalid_sibling,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client parent = { 0 };
    struct export_state parent_state = { 0 };
    struct export_state child_state = { 0 };
    struct remote_state remote_state = { 0 };
    if (!client_connect(&connection, socket_name))
        return 1;
    if (!xdg_toplevel_client_create(&connection, &parent)) {
        fprintf(stderr, "remote-subsurface: failed to create parent toplevel\n");
        client_disconnect(&connection);
        return 1;
    }
    struct treeland_remote_subsurface_manager_v1 *manager = client_bind(&connection,
        "treeland_remote_subsurface_manager_v1",
        &treeland_remote_subsurface_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "remote-subsurface: missing global\n");
        return 1;
    }
    struct treeland_exported_surface_v1 *parent_exported =
        treeland_remote_subsurface_manager_v1_export_surface(manager, parent.surface);
    struct wl_surface *child_surface = wl_compositor_create_surface(parent.compositor);
    if (!child_surface) {
        fprintf(stderr, "remote-subsurface: child surface creation failed\n");
        return 1;
    }
    struct treeland_exported_surface_v1 *child_exported =
        treeland_remote_subsurface_manager_v1_export_surface(manager, child_surface);
    if (!parent_exported || !child_exported) {
        fprintf(stderr, "remote-subsurface: export_surface failed\n");
        return 1;
    }
    treeland_exported_surface_v1_add_listener(parent_exported, &exported_listener, &parent_state);
    treeland_exported_surface_v1_add_listener(child_exported, &exported_listener, &child_state);
    if (wl_display_roundtrip(connection.display) < 0 || !parent_state.token[0]
        || !child_state.token[0]) {
        fprintf(stderr, "remote-subsurface: no surface token received\n");
        return 1;
    }
    struct treeland_remote_subsurface_v1 *invalid_remote =
        treeland_exported_surface_v1_create_remote_subsurface(child_exported, "missing-parent");
    if (!invalid_remote || wl_display_roundtrip(connection.display) < 0
        || child_state.parent_rejected != 1
        || child_state.rejected_reason
            != TREELAND_EXPORTED_SURFACE_V1_REJECT_REASON_INVALID_TOKEN
        || strcmp(child_state.rejected_token, "missing-parent") != 0) {
        fprintf(stderr, "remote-subsurface: invalid parent was not rejected\n");
        return 1;
    }
    struct treeland_remote_subsurface_v1 *remote =
        treeland_exported_surface_v1_create_remote_subsurface(child_exported, parent_state.token);
    if (!remote) {
        fprintf(stderr, "remote-subsurface: create_remote_subsurface failed\n");
        return 1;
    }
    treeland_remote_subsurface_v1_add_listener(remote, &remote_listener, &remote_state);
    treeland_remote_subsurface_v1_set_position(remote, -12, 34);
    treeland_remote_subsurface_v1_place_below(remote, "");
    if (wl_display_roundtrip(connection.display) < 0) {
        fprintf(stderr, "remote-subsurface: relationship update failed\n");
        return 1;
    }
    struct remote_subsurface_server_state server_state = { 0 };
    if (!invoke_on_server_thread(remote_subsurface_read_server_state, &server_state)
        || !server_state.valid || !server_state.parent_matches
        || server_state.type != 1 /* WSubsurface::Type::Remote */
        || server_state.place != 0 /* WSubsurface::Place::Below */
        || server_state.x != -12.0 || server_state.y != 34.0) {
        fprintf(stderr, "remote-subsurface: server state was not applied\n");
        return 1;
    }
    treeland_remote_subsurface_v1_place_above(remote, "missing-sibling");
    if (wl_display_roundtrip(connection.display) < 0 || remote_state.invalid_sibling != 1
        || strcmp(remote_state.sibling_token, "missing-sibling") != 0) {
        fprintf(stderr, "remote-subsurface: invalid sibling was not reported\n");
        return 1;
    }
    treeland_remote_subsurface_v1_destroy(remote);
    treeland_exported_surface_v1_destroy(child_exported);
    wl_surface_destroy(child_surface);
    treeland_exported_surface_v1_destroy(parent_exported);
    treeland_remote_subsurface_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&parent);
    client_disconnect(&connection);
    return 0;
}
