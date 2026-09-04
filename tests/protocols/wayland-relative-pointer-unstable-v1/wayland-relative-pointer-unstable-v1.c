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
#include <wlr/util/log.h>

static uint32_t g_caps;
static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps) {
	(void)data;
	(void)seat;
	g_caps = caps;
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) {
	(void)data;
	(void)seat;
	(void)name;
}
static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_caps,
	.name = seat_name,
};

static int read_server(struct relative_pointer_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(relative_pointer_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "relative-pointer: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "relative-pointer: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "relative-pointer: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	if (!(g_caps & 0x1)) {
		wlr_log(WLR_ERROR, "relative-pointer: seat has no pointer capability");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	struct wl_pointer *pointer = wl_seat_get_pointer(seat);
	if (pointer == NULL) {
		wlr_log(WLR_ERROR, "relative-pointer: wl_seat_get_pointer returned NULL");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_relative_pointer_manager_v1 *manager = client_bind(
		&conn, zwp_relative_pointer_manager_v1_interface.name, &zwp_relative_pointer_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "relative-pointer: failed to bind manager");
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_relative_pointer_v1 *rel =
		zwp_relative_pointer_manager_v1_get_relative_pointer(manager, pointer);
	if (rel == NULL) {
		wlr_log(WLR_ERROR, "relative-pointer: get_relative_pointer returned NULL");
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
		wlr_log(WLR_ERROR, "relative-pointer: manager not found");
		failed = 1;
	} else if (srv.count < 1) {
		wlr_log(WLR_ERROR, "relative-pointer: expected count >= 1, got %d", srv.count);
		failed = 1;
	}

	zwp_relative_pointer_v1_destroy(rel);
	zwp_relative_pointer_manager_v1_destroy(manager);
	wl_pointer_destroy(pointer);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
