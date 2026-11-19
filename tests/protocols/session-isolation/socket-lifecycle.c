// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "socket-lifecycle.h"
#include "client-connection.h"
#include "server-bridge-api.h"

#include <stdio.h>

#define REQUIRE(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", message); \
        goto cleanup; \
    } \
} while (0)

int protocol_test_run(const char *socket_name)
{
    struct socket_lifecycle_state state = {0};
    struct client_connection unaffected = {0}, original = {0}, replacement = {0}, descendant = {0};
    int result = 1;

    REQUIRE(invoke_on_server_thread(socket_lifecycle_prepare, &state), "prepare socket fixture");
    if (state.ready == 0) {
        fprintf(stderr, "SKIP   : D-Bus caller belongs to the global Session\n");
        return 77;
    }
    REQUIRE(state.ready == 1, "create a Session and listeners for the D-Bus caller");
    REQUIRE(client_connect(&unaffected, socket_name), "connect to the unrelated Session");
    REQUIRE(socket_lifecycle_register(FIRST_OWNER, state.original.fd),
        "register an original listener via D-Bus");
    REQUIRE(invoke_on_server_thread(socket_lifecycle_check_enable_sync, &state) && state.enable_synced,
        "registered socket enablement follows its Session rather than UserModel current user");
    REQUIRE(client_connect(&original, state.original.name), "connect to the original listener");
    REQUIRE(socket_lifecycle_register(FIRST_OWNER, state.original.fd)
        && wl_display_roundtrip(original.display) >= 0, "repeat registration preserves the client");
    REQUIRE(socket_lifecycle_register(SECOND_OWNER, state.original.fd)
        && socket_lifecycle_disconnect_owner(FIRST_OWNER)
        && wl_display_roundtrip(original.display) >= 0, "listener handover preserves the client after the old owner exits");
    REQUIRE(socket_lifecycle_disconnect_owner(SECOND_OWNER)
        && wl_display_roundtrip(original.display) < 0, "current owner exit releases the listener and disconnects its client");
    client_disconnect(&original);

    REQUIRE(socket_lifecycle_register(SECOND_OWNER, state.original.fd)
        && socket_lifecycle_register(FIRST_OWNER, state.replacement.fd)
        && client_connect(&replacement, state.replacement.name), "register a replacement from a new owner");
    REQUIRE(socket_lifecycle_disconnect_owner(SECOND_OWNER)
        && wl_display_roundtrip(replacement.display) >= 0, "old owner exit preserves the replacement");

    REQUIRE(invoke_on_server_thread(socket_lifecycle_add_descendant, &state)
        && state.descendant[0] && client_connect(&descendant, state.descendant), "connect through a descendant listener");
    REQUIRE(invoke_on_server_thread(socket_lifecycle_logout, &state) && state.released,
        "logout releases the registered socket");
    REQUIRE(wl_display_roundtrip(descendant.display) < 0
        && wl_display_roundtrip(replacement.display) < 0, "logout disconnects both departing clients");
    REQUIRE(wl_display_roundtrip(unaffected.display) >= 0, "logout preserves the unrelated Session client");
    result = 0;

cleanup:
    if (descendant.display)
        client_disconnect(&descendant);
    if (replacement.display)
        client_disconnect(&replacement);
    if (original.display)
        client_disconnect(&original);
    if (unaffected.display)
        client_disconnect(&unaffected);
    if (!invoke_on_server_thread(socket_lifecycle_cleanup, NULL))
        result = 1;
    return result;
}
