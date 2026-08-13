/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */

#include "protocol-test-client.h"
#include "treeland-input-manager-unstable-v1-client-protocol.h"

int protocol_test_run(const char *socket_name)
{
    struct protocol_test_connection connection;
    if (!protocol_test_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = protocol_test_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct treeland_input_manager_v1 *manager = protocol_test_bind(
        &connection, "treeland_input_manager_v1", &treeland_input_manager_v1_interface, 1);
    if (!seat || !manager)
        goto failed;

    if (wl_display_roundtrip(connection.display) < 0) {
        goto failed;
    }

    treeland_input_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    protocol_test_disconnect(&connection);
    return 0;

failed:
    protocol_test_disconnect(&connection);
    return 1;
}
