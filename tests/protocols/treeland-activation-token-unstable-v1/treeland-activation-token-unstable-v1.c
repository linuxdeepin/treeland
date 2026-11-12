// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "activation-token-fixture.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-activation-v1-client-protocol.h"
#include "xdg-toplevel-client.h"
#include "treeland-activation-token-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

#define MANAGER_INTERFACE "treeland_activation_token_manager_v1"

/* ActivationManagerInterfaceV1::TokenDisposition */
enum { DISPOSITION_INVALID = 0, DISPOSITION_ATTENTION = 1, DISPOSITION_ACTIVE = 2 };

enum { MAX_TOKENS = 4, TOKEN_MAX = 128 };

struct token_state {
    int count;
    char tokens[MAX_TOKENS][TOKEN_MAX];
};

static void handle_token(void *data,
                         struct treeland_activation_token_v1 *token_object,
                         const char *token)
{
    (void)token_object;
    struct token_state *state = data;
    if (state->count < MAX_TOKENS)
        snprintf(state->tokens[state->count], TOKEN_MAX, "%s", token);
    state->count++;
}

static const struct treeland_activation_token_v1_listener token_listener = {
    .token = handle_token,
};

// Every request_token mints an independent token, and the minted token must
// resolve to a real activation (Active, never a downgrade to Attention).
static int check_accepted(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    int result = 1;
    struct treeland_activation_token_manager_v1 *manager = NULL;
    struct treeland_activation_token_v1 *token_one = NULL;
    struct treeland_activation_token_v1 *token_two = NULL;
    struct xdg_activation_v1 *activation = NULL;
    struct xdg_toplevel_client toplevel = { 0 };
    int toplevel_created = 0;

    manager = client_bind(&connection, MANAGER_INTERFACE,
                          &treeland_activation_token_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "activation-token: manager global missing\n");
        goto done;
    }
    if (wl_display_roundtrip(connection.display) == -1) {
        fprintf(stderr, "activation-token: manager bind was rejected\n");
        goto done;
    }

    struct token_state state = { 0 };
    token_one = treeland_activation_token_manager_v1_request_token(manager);
    token_two = treeland_activation_token_manager_v1_request_token(manager);
    if (!token_one || !token_two) {
        fprintf(stderr, "activation-token: request_token proxy failed\n");
        goto done;
    }
    treeland_activation_token_v1_add_listener(token_one, &token_listener, &state);
    treeland_activation_token_v1_add_listener(token_two, &token_listener, &state);

    if (wl_display_roundtrip(connection.display) == -1) {
        fprintf(stderr, "activation-token: roundtrip failed after request_token\n");
        goto done;
    }
    if (state.count != 2 || state.tokens[0][0] == '\0' || state.tokens[1][0] == '\0') {
        fprintf(stderr, "activation-token: expected two tokens, got %d\n", state.count);
        goto done;
    }
    if (strcmp(state.tokens[0], state.tokens[1]) == 0) {
        fprintf(stderr, "activation-token: both request_token calls returned the same token\n");
        goto done;
    }

    // The minted token must reach the production activation pipeline as a real
    // activation, so consume it through xdg_activation_v1 against a mapped
    // toplevel and read the disposition back from the compositor.
    activation = client_bind(&connection, xdg_activation_v1_interface.name,
                             &xdg_activation_v1_interface,
                             xdg_activation_v1_interface.version);
    if (!activation) {
        fprintf(stderr, "activation-token: xdg_activation_v1 bind failed\n");
        goto done;
    }

    if (!xdg_toplevel_client_create_pending(&connection, &toplevel)) {
        fprintf(stderr, "activation-token: toplevel create_pending failed\n");
        goto done;
    }
    toplevel_created = 1;
    if (!xdg_toplevel_client_complete_map(&connection, &toplevel)) {
        fprintf(stderr, "activation-token: toplevel complete_map failed\n");
        goto done;
    }

    xdg_activation_v1_activate(activation, state.tokens[0], toplevel.surface);
    if (wl_display_roundtrip(connection.display) == -1) {
        fprintf(stderr, "activation-token: roundtrip failed after activate\n");
        goto done;
    }

    struct activation_token_server_state server_state;
    if (TEST_READ_SERVER(activation_token_read_server_state, &server_state,
                         "activation-token: failed to read server state")) {
        goto done;
    }
    if (!server_state.activate_requested) {
        fprintf(stderr, "activation-token: no activateRequested captured\n");
        goto done;
    }
    if (server_state.disposition != DISPOSITION_ACTIVE) {
        fprintf(stderr, "activation-token: expected disposition Active(%d), got %d\n",
                DISPOSITION_ACTIVE, server_state.disposition);
        goto done;
    }

    result = 0;

done:
    if (toplevel_created)
        xdg_toplevel_client_destroy(&toplevel);
    if (activation)
        xdg_activation_v1_destroy(activation);
    if (token_one)
        treeland_activation_token_v1_destroy(token_one);
    if (token_two)
        treeland_activation_token_v1_destroy(token_two);
    if (manager)
        treeland_activation_token_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return result;
}

// Each Session owns its instance, so a token minted for one user is not in the
// other user's token table and consumes there as Invalid.
static int check_cross_session(const char *socket_name)
{
    struct activation_token_server_state server_state;
    if (TEST_READ_SERVER(activation_token_read_server_state, &server_state,
                         "activation-token: failed to read server state"))
        return 1;
    if (!server_state.second_session) {
        fprintf(stderr, "activation-token: no second session available, cross-session check skipped\n");
        return 0;
    }

    int result = 1;
    struct client_connection holder_a;
    struct client_connection holder_b;
    struct client_connection consumer;
    int holder_a_connected = 0;
    int holder_b_connected = 0;
    int consumer_connected = 0;
    struct treeland_activation_token_manager_v1 *manager_a = NULL;
    struct treeland_activation_token_manager_v1 *manager_b = NULL;
    struct treeland_activation_token_v1 *token_a = NULL;
    struct treeland_activation_token_v1 *token_b = NULL;
    struct xdg_activation_v1 *activation = NULL;
    struct xdg_toplevel_client toplevel = { 0 };
    int toplevel_created = 0;
    struct token_state state_a = { 0 };
    struct token_state state_b = { 0 };

    // Session A: the first user binds its manager and mints a token.
    if (!client_connect(&holder_a, socket_name))
        return 1;
    holder_a_connected = 1;
    manager_a = client_bind(&holder_a, MANAGER_INTERFACE,
                            &treeland_activation_token_manager_v1_interface, 1);
    if (!manager_a || wl_display_roundtrip(holder_a.display) == -1) {
        fprintf(stderr, "activation-token: first session could not bind its manager\n");
        goto done;
    }
    token_a = treeland_activation_token_manager_v1_request_token(manager_a);
    treeland_activation_token_v1_add_listener(token_a, &token_listener, &state_a);
    if (wl_display_roundtrip(holder_a.display) == -1 || state_a.count != 1
        || state_a.tokens[0][0] == '\0') {
        fprintf(stderr, "activation-token: first session did not mint a token\n");
        goto done;
    }

    // Session B: the second user's client binds its own manager and mints a
    // token, proving the per-session instances coexist.
    if (!client_connect(&holder_b, server_state.second_socket)) {
        fprintf(stderr, "activation-token: second session socket is not connectable\n");
        goto done;
    }
    holder_b_connected = 1;
    manager_b = client_bind(&holder_b, MANAGER_INTERFACE,
                            &treeland_activation_token_manager_v1_interface, 1);
    if (!manager_b) {
        fprintf(stderr, "activation-token: manager missing in the second session\n");
        goto done;
    }
    if (wl_display_roundtrip(holder_b.display) == -1) {
        fprintf(stderr, "activation-token: second session could not bind its manager\n");
        goto done;
    }
    token_b = treeland_activation_token_manager_v1_request_token(manager_b);
    treeland_activation_token_v1_add_listener(token_b, &token_listener, &state_b);
    if (wl_display_roundtrip(holder_b.display) == -1 || state_b.count != 1
        || state_b.tokens[0][0] == '\0') {
        fprintf(stderr, "activation-token: second session did not mint a token\n");
        goto done;
    }

    // The first session's token is not in the second session's table: consuming
    // it there must resolve to Invalid instead of activating a window.
    if (!client_connect(&consumer, server_state.second_socket))
        goto done;
    consumer_connected = 1;
    activation = client_bind(&consumer, xdg_activation_v1_interface.name,
                             &xdg_activation_v1_interface,
                             xdg_activation_v1_interface.version);
    if (!activation) {
        fprintf(stderr, "activation-token: xdg_activation_v1 bind failed in the second session\n");
        goto done;
    }
    if (!xdg_toplevel_client_create_pending(&consumer, &toplevel)
        || !xdg_toplevel_client_complete_map(&consumer, &toplevel)) {
        fprintf(stderr, "activation-token: second-session toplevel failed\n");
        goto done;
    }
    toplevel_created = 1;
    xdg_activation_v1_activate(activation, state_a.tokens[0], toplevel.surface);
    if (wl_display_roundtrip(consumer.display) == -1) {
        fprintf(stderr, "activation-token: roundtrip failed after the cross-session activate\n");
        goto done;
    }
    if (TEST_READ_SERVER(activation_token_read_server_state, &server_state,
                         "activation-token: failed to read server state"))
        goto done;
    if (server_state.disposition != DISPOSITION_INVALID) {
        fprintf(stderr,
                "activation-token: a first-session token resolved in the second session (disposition %d)\n",
                server_state.disposition);
        goto done;
    }

    result = 0;

done:
    if (toplevel_created)
        xdg_toplevel_client_destroy(&toplevel);
    if (activation)
        xdg_activation_v1_destroy(activation);
    if (token_a)
        treeland_activation_token_v1_destroy(token_a);
    if (token_b)
        treeland_activation_token_v1_destroy(token_b);
    if (consumer_connected)
        client_disconnect(&consumer);
    if (holder_b_connected) {
        if (manager_b)
            treeland_activation_token_manager_v1_destroy(manager_b);
        client_disconnect(&holder_b);
    }
    if (holder_a_connected) {
        if (manager_a)
            treeland_activation_token_manager_v1_destroy(manager_a);
        client_disconnect(&holder_a);
    }
    return result;
}

int protocol_test_run(const char *socket_name)
{
    if (check_accepted(socket_name) != 0)
        return 1;

    return check_cross_session(socket_name);
}
