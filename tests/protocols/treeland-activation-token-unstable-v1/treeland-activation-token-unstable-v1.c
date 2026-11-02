// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "activation-token-fixture.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-activation-v1-client-protocol.h"
#include "xdg-toplevel-client.h"
#include "treeland-activation-token-unstable-v1-client-protocol.h"
#include "treeland-prelaunch-splash-unstable-v2-client-protocol.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MANAGER_INTERFACE "treeland_activation_token_manager_v1"
#define SPLASH_MANAGER_INTERFACE "treeland_prelaunch_splash_manager_v2"

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

// Both manager globals reject at bind time, so the observable result is a
// protocol error on the bind's new object. Assert the error code and that it
// was raised on the interface that was being bound, not somewhere else.
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

// A client that does not implement treeland-prelaunch-splash-unstable-v2 is not
// the application manager, so binding the manager global itself is rejected.
static int check_not_privileged(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    int result = 1;
    const struct wl_interface *interface = NULL;
    uint32_t code = 0;

    struct treeland_activation_token_manager_v1 *manager =
        client_bind(&connection, MANAGER_INTERFACE,
                    &treeland_activation_token_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "activation-token: manager global missing\n");
        goto done;
    }

    if (!expect_bind_rejected(&connection,
                              &treeland_activation_token_manager_v1_interface,
                              TREELAND_ACTIVATION_TOKEN_MANAGER_V1_ERROR_NOT_PRIVILEGED)) {
        wl_display_get_protocol_error(connection.display, &interface, &code);
        fprintf(stderr,
                "activation-token: expected EPROTO/not_privileged on the token manager, got %u on %s\n",
                code, interface ? interface->name : "(none)");
        goto done;
    }

    result = 0;

done:
    client_disconnect(&connection);
    return result;
}

// Releases both singletons and confirms the server processed the destroy
// requests, so the next connection does not race the disconnect.
static int release_managers(struct client_connection *connection,
                            struct treeland_activation_token_manager_v1 **manager,
                            struct treeland_prelaunch_splash_manager_v2 **splash)
{
    if (manager && *manager) {
        treeland_activation_token_manager_v1_destroy(*manager);
        *manager = NULL;
    }
    if (splash && *splash) {
        treeland_prelaunch_splash_manager_v2_destroy(*splash);
        *splash = NULL;
    }
    return wl_display_roundtrip(connection->display) >= 0;
}

// The application manager: it implements treeland-prelaunch-splash-unstable-v2
// and must bind that global before the activation token manager. Every
// request_token mints an independent token, and the minted token must resolve
// to a real activation (Active, never a downgrade to Attention).
static int check_accepted(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    int result = 1;
    struct treeland_prelaunch_splash_manager_v2 *splash = NULL;
    struct treeland_activation_token_manager_v1 *manager = NULL;
    struct treeland_activation_token_v1 *token_one = NULL;
    struct treeland_activation_token_v1 *token_two = NULL;
    struct xdg_activation_v1 *activation = NULL;
    struct xdg_toplevel_client toplevel = { 0 };
    int toplevel_created = 0;

    splash = client_bind(&connection, SPLASH_MANAGER_INTERFACE,
                         &treeland_prelaunch_splash_manager_v2_interface, 1);
    if (!splash) {
        fprintf(stderr, "activation-token: splash manager global missing\n");
        goto done;
    }
    if (wl_display_roundtrip(connection.display) == -1) {
        fprintf(stderr, "activation-token: splash manager bind was rejected\n");
        goto done;
    }

    manager = client_bind(&connection, MANAGER_INTERFACE,
                          &treeland_activation_token_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "activation-token: manager global missing\n");
        goto done;
    }
    if (wl_display_roundtrip(connection.display) == -1) {
        fprintf(stderr, "activation-token: token manager bind was rejected after the splash\n");
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
    if (!release_managers(&connection, &manager, &splash))
        result = 1;
    client_disconnect(&connection);
    return result;
}

// The token manager is a per-session singleton: while the application manager
// holds it, a second client is rejected with binding_rejected. After the holder
// releases the manager, another client that implements the splash manager can
// bind it.
static int check_token_manager_singleton(const char *socket_name)
{
    struct client_connection holder;
    struct client_connection second;
    struct client_connection third;
    int holder_connected = 0;
    int second_connected = 0;
    int third_connected = 0;

    int result = 1;
    struct treeland_prelaunch_splash_manager_v2 *holder_splash = NULL;
    struct treeland_activation_token_manager_v1 *holder_manager = NULL;
    struct treeland_activation_token_manager_v1 *second_manager = NULL;
    struct treeland_prelaunch_splash_manager_v2 *third_splash = NULL;
    struct treeland_activation_token_manager_v1 *third_manager = NULL;
    const struct wl_interface *interface = NULL;
    uint32_t code = 0;

    if (!client_connect(&holder, socket_name))
        return 1;
    holder_connected = 1;

    holder_splash = client_bind(&holder, SPLASH_MANAGER_INTERFACE,
                                &treeland_prelaunch_splash_manager_v2_interface, 1);
    if (!holder_splash || wl_display_roundtrip(holder.display) == -1) {
        fprintf(stderr, "activation-token: holder splash manager bind failed\n");
        goto done;
    }
    holder_manager = client_bind(&holder, MANAGER_INTERFACE,
                                 &treeland_activation_token_manager_v1_interface, 1);
    if (!holder_manager || wl_display_roundtrip(holder.display) == -1) {
        fprintf(stderr, "activation-token: holder token manager bind failed\n");
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

    if (!release_managers(&holder, &holder_manager, &holder_splash)) {
        fprintf(stderr, "activation-token: holder failed to release the managers\n");
        goto done;
    }

    if (!client_connect(&third, socket_name))
        goto done;
    third_connected = 1;
    third_splash = client_bind(&third, SPLASH_MANAGER_INTERFACE,
                               &treeland_prelaunch_splash_manager_v2_interface, 1);
    third_manager = client_bind(&third, MANAGER_INTERFACE,
                                &treeland_activation_token_manager_v1_interface, 1);
    if (!third_splash || !third_manager) {
        fprintf(stderr, "activation-token: globals missing for the client after release\n");
        goto done;
    }
    if (wl_display_roundtrip(third.display) == -1) {
        fprintf(stderr, "activation-token: released managers were not rebindable\n");
        goto done;
    }

    result = 0;

done:
    if (third_connected) {
        if (!release_managers(&third, &third_manager, &third_splash))
            result = 1;
        client_disconnect(&third);
    }
    if (second_connected)
        client_disconnect(&second);
    if (holder_connected) {
        if (!release_managers(&holder, &holder_manager, &holder_splash))
            result = 1;
        client_disconnect(&holder);
    }
    return result;
}

// treeland_prelaunch_splash_manager_v2 is a per-session singleton too: the
// second bind is rejected and destroying the manager releases the binding.
static int check_splash_manager_singleton(const char *socket_name)
{
    struct client_connection first;
    struct client_connection second;
    struct client_connection third;
    int first_connected = 0;
    int second_connected = 0;
    int third_connected = 0;

    int result = 1;
    struct treeland_prelaunch_splash_manager_v2 *first_splash = NULL;
    struct treeland_prelaunch_splash_manager_v2 *second_splash = NULL;
    struct treeland_prelaunch_splash_manager_v2 *third_splash = NULL;
    const struct wl_interface *interface = NULL;
    uint32_t code = 0;

    if (!client_connect(&first, socket_name))
        return 1;
    first_connected = 1;
    first_splash = client_bind(&first, SPLASH_MANAGER_INTERFACE,
                               &treeland_prelaunch_splash_manager_v2_interface, 1);
    if (!first_splash || wl_display_roundtrip(first.display) == -1) {
        fprintf(stderr, "activation-token: first splash manager bind failed\n");
        goto done;
    }

    if (!client_connect(&second, socket_name))
        goto done;
    second_connected = 1;
    second_splash = client_bind(&second, SPLASH_MANAGER_INTERFACE,
                                &treeland_prelaunch_splash_manager_v2_interface, 1);
    if (!second_splash) {
        fprintf(stderr, "activation-token: splash manager global missing on the second client\n");
        goto done;
    }
    if (!expect_bind_rejected(&second,
                              &treeland_prelaunch_splash_manager_v2_interface,
                              TREELAND_PRELAUNCH_SPLASH_MANAGER_V2_ERROR_BINDING_REJECTED)) {
        wl_display_get_protocol_error(second.display, &interface, &code);
        fprintf(stderr,
                "activation-token: second splash bind expected binding_rejected, got %u on %s\n",
                code, interface ? interface->name : "(none)");
        goto done;
    }

    if (!release_managers(&first, NULL, &first_splash)) {
        fprintf(stderr, "activation-token: splash manager could not be destroyed\n");
        goto done;
    }

    if (!client_connect(&third, socket_name))
        goto done;
    third_connected = 1;
    third_splash = client_bind(&third, SPLASH_MANAGER_INTERFACE,
                               &treeland_prelaunch_splash_manager_v2_interface, 1);
    if (!third_splash) {
        fprintf(stderr, "activation-token: splash manager global missing after release\n");
        goto done;
    }
    if (wl_display_roundtrip(third.display) == -1) {
        fprintf(stderr, "activation-token: released splash manager was not rebindable\n");
        goto done;
    }

    result = 0;

done:
    if (third_connected) {
        if (!release_managers(&third, NULL, &third_splash))
            result = 1;
        client_disconnect(&third);
    }
    if (second_connected)
        client_disconnect(&second);
    if (first_connected) {
        if (!release_managers(&first, NULL, &first_splash))
            result = 1;
        client_disconnect(&first);
    }
    return result;
}

int protocol_test_run(const char *socket_name)
{
    if (check_not_privileged(socket_name) != 0)
        return 1;

    if (check_accepted(socket_name) != 0)
        return 1;

    if (check_token_manager_singleton(socket_name) != 0)
        return 1;

    return check_splash_manager_singleton(socket_name);
}
