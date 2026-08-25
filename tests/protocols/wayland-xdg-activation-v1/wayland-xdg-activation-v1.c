// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the xdg_activation_v1 global served by Treeland's
// ActivationManagerInterfaceV1 wrapper.  The client requests an activation
// token and must observe the token.done event carrying a non-empty token
// string, exercising the real token-mint path.

#include "client-connection.h"
#include "xdg-activation-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct activation_state {
    int done;
    char token[256];
};

static void done(void *data, struct xdg_activation_token_v1 *token,
                 const char *token_string)
{
    (void)token;
    struct activation_state *state = data;
    state->done = 1;
    if (token_string) {
        strncpy(state->token, token_string, sizeof(state->token) - 1);
        state->token[sizeof(state->token) - 1] = '\0';
    }
}

static const struct xdg_activation_token_v1_listener token_listener = {
    .done = done,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "xdg-activation: connect failed\n");
        return 1;
    }

    struct xdg_activation_v1 *activation = client_bind(
        &conn, "xdg_activation_v1", &xdg_activation_v1_interface, 1);
    if (!activation) {
        fprintf(stderr, "xdg-activation: failed to bind xdg_activation_v1\n");
        client_disconnect(&conn);
        return 1;
    }

    struct activation_state state;
    memset(&state, 0, sizeof(state));
    struct xdg_activation_token_v1 *token =
        xdg_activation_v1_get_activation_token(activation);
    if (!token) {
        fprintf(stderr, "xdg-activation: get_activation_token returned NULL\n");
        xdg_activation_v1_destroy(activation);
        client_disconnect(&conn);
        return 1;
    }
    xdg_activation_token_v1_add_listener(token, &token_listener, &state);
    xdg_activation_token_v1_commit(token);

    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (!state.done) {
        fprintf(stderr, "xdg-activation: no token.done event received\n");
        failed = 1;
    } else if (state.token[0] == '\0') {
        fprintf(stderr, "xdg-activation: done event carried an empty token\n");
        failed = 1;
    }

    xdg_activation_token_v1_destroy(token);
    xdg_activation_v1_destroy(activation);
    client_disconnect(&conn);
    return failed;
}
