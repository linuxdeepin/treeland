// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "activation-token-fixture.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-activation-v1-client-protocol.h"
#include "xdg-toplevel-client.h"
#include "treeland-activation-token-unstable-v1-client-protocol.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

// The manager rejects a second concurrent bind at bind time, so the observable
// result is a protocol error on the bind's new object. Assert the error code
// and that it was raised on the interface that was being bound, not elsewhere.
static int expect_bind_rejected(struct client_connection *connection,
                                const struct wl_interface *expected_interface,
                                uint32_t expected_code)
{
    const struct wl_interface *interface = NULL;
    uint32_t object_id = 0;

    if (wl_display_roundtrip(connection->display) != -1)
        return 0;
    if (wl_display_get_error(connection->display) != EPROTO)
        return 0;
    return wl_display_get_protocol_error(connection->display, &interface, &object_id)
               == expected_code
        && interface == expected_interface;
}

// Any client of the session may bind the manager: the compositor does not
// verify the caller's identity yet (appid-based authentication is a TODO in
// the implementation). Binding without any prior privilege must succeed.
static int check_unprivileged_bind(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    int result = 1;
    struct treeland_activation_token_manager_v1 *manager =
        client_bind(&connection, MANAGER_INTERFACE,
                    &treeland_activation_token_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "activation-token: manager global missing\n");
        goto done;
    }
    if (wl_display_roundtrip(connection.display) == -1) {
        fprintf(stderr, "activation-token: bind was rejected for an unprivileged client\n");
        goto done;
    }

    result = 0;

done:
    if (manager)
        treeland_activation_token_manager_v1_destroy(manager);
    if (result != 0 || wl_display_roundtrip(connection.display) == -1)
        result = 1;
    client_disconnect(&connection);
    return result;
}

// Releases the singleton and confirms the server processed the destroy
// request, so the next connection does not race the disconnect.
static int release_manager(struct client_connection *connection,
                           struct treeland_activation_token_manager_v1 **manager)
{
    if (manager && *manager) {
        treeland_activation_token_manager_v1_destroy(*manager);
        *manager = NULL;
    }
    return wl_display_roundtrip(connection->display) >= 0;
}

// Rebinding after a client disappeared without destroying its manager proves
// the compositor released the per-session singleton on disconnect. The
// teardown happens when the compositor reads the closed socket, which is
// asynchronous to our client_disconnect(), so poll with fresh connections: each
// rejected attempt still saw the slot held, the first accepted one saw it
// released. That connection releases it again so later checks start clean.
static int rebind_after_disconnect(const char *socket_name)
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        struct client_connection connection;
        if (!client_connect(&connection, socket_name))
            return 0;

        struct treeland_activation_token_manager_v1 *manager =
            client_bind(&connection, MANAGER_INTERFACE,
                        &treeland_activation_token_manager_v1_interface, 1);
        if (!manager || wl_display_roundtrip(connection.display) == -1) {
            client_disconnect(&connection);
            usleep(10000);
            continue;
        }

        int released = release_manager(&connection, &manager);
        client_disconnect(&connection);
        return released;
    }
    return 0;
}

// The client dying without destroying anything is the crash and restart path:
// the disconnect alone must release the per-session singleton.
static int check_disconnect_releases(const char *socket_name)
{
    struct client_connection holder;
    if (!client_connect(&holder, socket_name))
        return 1;

    struct treeland_activation_token_manager_v1 *manager =
        client_bind(&holder, MANAGER_INTERFACE,
                    &treeland_activation_token_manager_v1_interface, 1);
    if (!manager || wl_display_roundtrip(holder.display) == -1) {
        fprintf(stderr, "activation-token: holder could not bind the manager\n");
        client_disconnect(&holder);
        return 1;
    }

    // No destroy: this is exactly what a crashing client does, and the proxy
    // above is intentionally left alive so no destroy is sent.
    client_disconnect(&holder);

    if (!rebind_after_disconnect(socket_name)) {
        fprintf(stderr, "activation-token: manager stayed bound after a disconnect\n");
        return 1;
    }

    return 0;
}

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
    if (!release_manager(&connection, &manager))
        result = 1;
    client_disconnect(&connection);
    return result;
}

// The token manager is a per-session singleton: while one client holds it, a
// second client is rejected with binding_rejected. After the holder releases
// the manager, another client can bind it.
static int check_token_manager_singleton(const char *socket_name)
{
    struct client_connection holder;
    struct client_connection second;
    struct client_connection third;
    int holder_connected = 0;
    int second_connected = 0;
    int third_connected = 0;

    int result = 1;
    struct treeland_activation_token_manager_v1 *holder_manager = NULL;
    struct treeland_activation_token_manager_v1 *second_manager = NULL;
    struct treeland_activation_token_manager_v1 *third_manager = NULL;
    const struct wl_interface *interface = NULL;
    uint32_t code = 0;

    if (!client_connect(&holder, socket_name))
        return 1;
    holder_connected = 1;

    holder_manager = client_bind(&holder, MANAGER_INTERFACE,
                                 &treeland_activation_token_manager_v1_interface, 1);
    if (!holder_manager || wl_display_roundtrip(holder.display) == -1) {
        fprintf(stderr, "activation-token: holder manager bind failed\n");
        goto done;
    }

    if (!client_connect(&second, socket_name))
        goto done;
    second_connected = 1;
    second_manager = client_bind(&second, MANAGER_INTERFACE,
                                 &treeland_activation_token_manager_v1_interface, 1);
    if (!second_manager) {
        fprintf(stderr, "activation-token: manager global missing on the second client\n");
        goto done;
    }
    if (!expect_bind_rejected(&second,
                              &treeland_activation_token_manager_v1_interface,
                              TREELAND_ACTIVATION_TOKEN_MANAGER_V1_ERROR_BINDING_REJECTED)) {
        wl_display_get_protocol_error(second.display, &interface, &code);
        fprintf(stderr,
                "activation-token: second bind expected binding_rejected, got %u on %s\n",
                code, interface ? interface->name : "(none)");
        goto done;
    }

    if (!release_manager(&holder, &holder_manager)) {
        fprintf(stderr, "activation-token: holder failed to release the manager\n");
        goto done;
    }

    if (!client_connect(&third, socket_name))
        goto done;
    third_connected = 1;
    third_manager = client_bind(&third, MANAGER_INTERFACE,
                                &treeland_activation_token_manager_v1_interface, 1);
    if (!third_manager) {
        fprintf(stderr, "activation-token: manager global missing for the client after release\n");
        goto done;
    }
    if (wl_display_roundtrip(third.display) == -1) {
        fprintf(stderr, "activation-token: released manager was not rebindable\n");
        goto done;
    }

    result = 0;

done:
    if (third_connected) {
        if (!release_manager(&third, &third_manager))
            result = 1;
        client_disconnect(&third);
    }
    if (second_connected)
        client_disconnect(&second);
    if (holder_connected) {
        if (!release_manager(&holder, &holder_manager))
            result = 1;
        client_disconnect(&holder);
    }
    return result;
}

// Each Session owns its instance, so one user's client never blocks another
// user's, and a token minted for one user is not in the other user's table.
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

    // Session B: the second user's client binds while the first user's is
    // still held. A server-wide singleton would reject this bind.
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
        fprintf(stderr, "activation-token: second session was rejected while the first held its manager\n");
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
        if (!release_manager(&holder_b, &manager_b))
            result = 1;
        client_disconnect(&holder_b);
    }
    if (holder_a_connected) {
        if (!release_manager(&holder_a, &manager_a))
            result = 1;
        client_disconnect(&holder_a);
    }
    return result;
}

int protocol_test_run(const char *socket_name)
{
    if (check_unprivileged_bind(socket_name) != 0)
        return 1;

    if (check_accepted(socket_name) != 0)
        return 1;

    if (check_token_manager_singleton(socket_name) != 0)
        return 1;

    if (check_disconnect_releases(socket_name) != 0)
        return 1;

    return check_cross_session(socket_name);
}
