/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client creates a zwp_text_input_v1,
 * maps a toplevel to get keyboard focus, and calls activate(surface, seat).
 * The test reads back the production WTextInputV1's activate() signal state,
 * verifying it was emitted, proving the text-input activation request reached
 * the real compositor text-input pipeline.
 */

#include "wayland-text-input-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "text-input-unstable-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

static int read_server(struct text_input_v1_server_state *state) {
	return TEST_READ_SERVER(text_input_v1_read_server_state, state,
		"text-input-v1: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("text-input-v1: connect failed\n");
		return 1;
	}

	struct client_seat client_seat = { 0 };
	if (!client_bind_seat(&conn, wl_seat_interface.version, &client_seat)) {
		TEST_ERROR("text-input-v1: no wl_seat global\n");
		client_disconnect(&conn);
		return 1;
	}
	struct wl_seat *seat = client_seat.seat;

	/* Map a toplevel so we have a surface to activate text input on. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		TEST_ERROR("text-input-v1: create_pending failed\n");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		TEST_ERROR("text-input-v1: complete_map failed\n");
		xdg_toplevel_client_destroy(&tc);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	struct zwp_text_input_manager_v1 *manager =
		client_bind(&conn, zwp_text_input_manager_v1_interface.name, &zwp_text_input_manager_v1_interface, zwp_text_input_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("text-input-v1: failed to bind manager: %s\n", strerror(errno));
		xdg_toplevel_client_destroy(&tc);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_text_input_v1 *ti = zwp_text_input_manager_v1_create_text_input(manager);
	if (ti == NULL) {
		TEST_ERROR("text-input-v1: create_text_input returned NULL\n");
		zwp_text_input_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	zwp_text_input_v1_activate(ti, seat, tc.surface);
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production text input must have been activated. */
	struct text_input_v1_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		TEST_ERROR("text-input-v1: manager not found\n");
		failed = 1;
	} else if (!srv.activated) {
		TEST_ERROR("text-input-v1: activate signal not emitted\n");
		failed = 1;
	}

	zwp_text_input_v1_deactivate(ti, seat);
	zwp_text_input_v1_destroy(ti);
	zwp_text_input_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
