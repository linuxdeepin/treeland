/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client obtains a wl_pointer from the
 * pointer-capable seat and creates a zwp_relative_pointer_v1.  The test then
 * reads back the production WRelativePointerManagerV1's relative_pointers list
 * length, verifying it is at least 1, proving the relative pointer object was
 * created in the real compositor rather than merely surviving without a
 * protocol error.
 */

#include "wayland-relative-pointer-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "relative-pointer-unstable-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

static int read_server(struct relative_pointer_server_state *state) {
	return TEST_READ_SERVER(relative_pointer_read_server_state, state,
		"relative-pointer: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("relative-pointer: connect failed\n");
		return 1;
	}

	struct client_pointer_seat pointer_seat = { 0 };
	if (!client_bind_pointer_seat(&conn, wl_seat_interface.version, &pointer_seat)) {
		TEST_ERROR("relative-pointer: no pointer-capable wl_seat\n");
		client_disconnect(&conn);
		return 1;
	}
	struct wl_seat *seat = pointer_seat.seat;
	struct wl_pointer *pointer = pointer_seat.pointer;

	struct zwp_relative_pointer_manager_v1 *manager = client_bind(
		&conn, zwp_relative_pointer_manager_v1_interface.name, &zwp_relative_pointer_manager_v1_interface, zwp_relative_pointer_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("relative-pointer: failed to bind manager: %s\n", strerror(errno));
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_relative_pointer_v1 *rel =
		zwp_relative_pointer_manager_v1_get_relative_pointer(manager, pointer);
	if (rel == NULL) {
		TEST_ERROR("relative-pointer: get_relative_pointer returned NULL\n");
		zwp_relative_pointer_manager_v1_destroy(manager);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production manager must have at least 1 relative pointer. */
	struct relative_pointer_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		TEST_ERROR("relative-pointer: manager not found\n");
		failed = 1;
	} else if (srv.count < 1) {
		TEST_ERROR("relative-pointer: expected count >= 1, got %d\n", srv.count);
		failed = 1;
	}

	zwp_relative_pointer_v1_destroy(rel);
	zwp_relative_pointer_manager_v1_destroy(manager);
	wl_pointer_destroy(pointer);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
