/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client obtains a wl_pointer from the
 * pointer-capable seat, creates a wp_cursor_shape_device_v1, and calls
 * set_shape with WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT (1).  The test then
 * reads back the production WCursorShapeManagerV1's captured request_set_shape
 * event data, verifying the shape value matches, proving the cursor-shape
 * request reached the real compositor cursor pipeline.
 */

#include "wayland-cursor-shape-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "cursor-shape-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

static int read_server(struct cursor_shape_server_state *state) {
	return TEST_READ_SERVER(cursor_shape_read_server_state, state,
		"cursor-shape: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("cursor-shape: connect failed\n");
		return 1;
	}

	struct client_pointer_seat pointer_seat = { 0 };
	if (!client_bind_pointer_seat(&conn, wl_seat_interface.version, &pointer_seat)) {
		TEST_ERROR("cursor-shape: no pointer-capable wl_seat\n");
		client_disconnect(&conn);
		return 1;
	}
	struct wl_seat *seat = pointer_seat.seat;
	struct wl_pointer *pointer = pointer_seat.pointer;

	struct wp_cursor_shape_manager_v1 *manager =
		client_bind(&conn, wp_cursor_shape_manager_v1_interface.name, &wp_cursor_shape_manager_v1_interface, wp_cursor_shape_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("cursor-shape: failed to bind manager: %s\n", strerror(errno));
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct wp_cursor_shape_device_v1 *device =
		wp_cursor_shape_manager_v1_get_pointer(manager, pointer);
	if (device == NULL) {
		TEST_ERROR("cursor-shape: get_pointer returned NULL\n");
		wp_cursor_shape_manager_v1_destroy(manager);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	wp_cursor_shape_device_v1_set_shape(device, 0, 1 /* DEFAULT */);
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production manager must have captured shape == 1 (DEFAULT). */
	struct cursor_shape_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		TEST_ERROR("cursor-shape: no request_set_shape captured\n");
		failed = 1;
	} else if (srv.shape != 1) {
		TEST_ERROR("cursor-shape: expected shape 1 (DEFAULT), got %u\n", srv.shape);
		failed = 1;
	}

	wp_cursor_shape_device_v1_destroy(device);
	wp_cursor_shape_manager_v1_destroy(manager);
	wl_pointer_destroy(pointer);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
