/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */

#include "treeland-input-manager-unstable-v1.h"
#include "protocol-test-client.h"
#include "treeland-input-manager-unstable-v1-client-protocol.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*
 * In the headless desktop-integration fixture there are no physical input
 * devices.  The test verifies:
 *
 *   1. The manager global binds and the initial capability_available batch
 *      is delivered (type mask reflects actual devices -- zero in headless).
 *   2. get_keyboard_settings with a valid seat posts a protocol error
 *      ("No Keyboard!") because the headless backend has no keyboard.
 */

struct capability_state {
    uint32_t last_type;
    struct wl_seat *last_seat;
    int events;
};

static void capability_available(void *data,
                                 struct treeland_input_manager_v1 *manager,
                                 uint32_t type,
                                 struct wl_seat *seat)
{
    (void)manager;
    struct capability_state *state = data;
    state->last_type = type;
    state->last_seat = seat;
    ++state->events;
}

static void capability_unavailable(void *data,
                                   struct treeland_input_manager_v1 *manager,
                                   uint32_t type,
                                   struct wl_seat *seat)
{
    (void)manager;
    (void)type;
    (void)seat;
    /* no-op in headless */
    (void)data;
}

static const struct treeland_input_manager_v1_listener manager_listener = {
    .capability_available = capability_available,
    .capability_unavailable = capability_unavailable,
};

/*
 * After the server posts a protocol error the connection is dead.
 * Returns 1 if the roundtrip failed with EPROTO and the error matches
 * the expected interface and code, 0 otherwise.
 */
static int expect_protocol_error(struct wl_display *display,
                                 const struct wl_interface *expected_interface,
                                 uint32_t expected_code)
{
    if (wl_display_roundtrip(display) >= 0)
        return 0;
    if (errno != EPROTO)
        return 0;
    const struct wl_interface *error_interface = NULL;
    uint32_t error_id = 0;
    const uint32_t code =
        wl_display_get_protocol_error(display, &error_interface, &error_id);
    return code == expected_code && error_interface == expected_interface;
}

/* Case 1: bind manager, verify capability events on bind. */
static int test_bind_and_capabilities(const char *socket_name)
{
    struct protocol_test_connection connection;
    if (!protocol_test_connect(&connection, socket_name))
        return 0;

    /* Bind wl_seat first -- the server's bind_resource iterates
     * seat client resources and crashes if the client has no seat. */
    struct wl_seat *seat =
        protocol_test_bind(&connection, "wl_seat", &wl_seat_interface, 1);

    struct treeland_input_manager_v1 *manager =
        protocol_test_bind(&connection,
                           "treeland_input_manager_v1",
                           &treeland_input_manager_v1_interface,
                           1);
    if (!manager)
        goto fail;

    /* Register listener before the roundtrip that flushes pending bind
     * events (capability_available is sent in bind_resource on the server). */
    struct capability_state caps = {0};
    treeland_input_manager_v1_add_listener(manager, &manager_listener, &caps);

    /* Roundtrip to deliver the initial capability_available batch. */
    if (wl_display_roundtrip(connection.display) < 0)
        goto fail;

    treeland_input_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    protocol_test_disconnect(&connection);
    return 1;

fail:
    if (manager)
        treeland_input_manager_v1_destroy(manager);
    if (seat)
        wl_seat_destroy(seat);
    protocol_test_disconnect(&connection);
    return 0;
}

/* Case 2: get_keyboard_settings with valid seat posts "No Keyboard!" error
 * in headless mode (no keyboard device). */
static int test_no_keyboard_error(const char *socket_name)
{
    struct protocol_test_connection connection;
    if (!protocol_test_connect(&connection, socket_name))
        return 0;

    struct wl_seat *seat =
        protocol_test_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct treeland_input_manager_v1 *manager =
        protocol_test_bind(&connection,
                           "treeland_input_manager_v1",
                           &treeland_input_manager_v1_interface,
                           1);
    if (!seat || !manager)
        goto fail;

    /* Consume initial capability events. */
    struct capability_state caps = {0};
    treeland_input_manager_v1_add_listener(manager, &manager_listener, &caps);
    if (wl_display_roundtrip(connection.display) < 0)
        goto fail;

    /* Headless backend has no keyboard, so the server posts error 0
     * ("No Keyboard!") on the manager resource. */
    treeland_input_manager_v1_get_keyboard_settings(manager, seat);

    int ok = expect_protocol_error(connection.display,
                                   &treeland_input_manager_v1_interface, 0);
    /* Connection is dead after the protocol error. */
    return ok;

fail:
    if (seat)
        wl_seat_destroy(seat);
    if (manager)
        treeland_input_manager_v1_destroy(manager);
    protocol_test_disconnect(&connection);
    return 0;
}

int protocol_test_run(const char *socket_name)
{
    /* Case 1: bind and capabilities */
    if (!test_bind_and_capabilities(socket_name)) {
        fprintf(stderr, "input-manager: bind_and_capabilities failed\n");
        return 1;
    }

    /* Case 2: no keyboard error in headless */
    if (!test_no_keyboard_error(socket_name)) {
        fprintf(stderr, "input-manager: no_keyboard_error failed\n");
        return 1;
    }

    return 0;
}
