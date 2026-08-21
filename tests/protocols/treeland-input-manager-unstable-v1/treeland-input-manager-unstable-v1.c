// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "treeland-input-manager-unstable-v1-client-protocol.h"

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct treeland_input_manager_v1 *manager = client_bind(
        &connection, "treeland_input_manager_v1", &treeland_input_manager_v1_interface, 1);
    if (!seat || !manager)
        goto failed;

    if (wl_display_roundtrip(connection.display) < 0) {
        goto failed;
    }

    treeland_input_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    client_disconnect(&connection);
    return 0;

failed:
    client_disconnect(&connection);
    return 1;
}
