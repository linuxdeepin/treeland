/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client creates a zwp_text_input_v3,
 * calls enable + commit, then the test reads back the production
 * WTextInputV3's wlroots handle current_enabled field, verifying it is true,
 * proving the text-input enable+commit reached the real compositor
 * text-input pipeline rather than merely surviving without a protocol error.
 */

#include "wayland-text-input-unstable-v3.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "text-input-unstable-v3-client-protocol.h"

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

static int read_server(struct text_input_v3_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(text_input_v3_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "text-input-v3: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "text-input-v3: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "text-input-v3: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	struct zwp_text_input_manager_v3 *manager =
		client_bind(&conn, zwp_text_input_manager_v3_interface.name, &zwp_text_input_manager_v3_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "text-input-v3: failed to bind manager");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_text_input_v3 *ti = zwp_text_input_manager_v3_get_text_input(manager, seat);
	if (ti == NULL) {
		wlr_log(WLR_ERROR, "text-input-v3: get_text_input returned NULL");
		zwp_text_input_manager_v3_destroy(manager);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	/* Enable the text input and commit. */
	zwp_text_input_v3_enable(ti);
	zwp_text_input_v3_commit(ti);
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production text input must have current_enabled == true. */
	struct text_input_v3_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "text-input-v3: no WTextInputV3 captured");
		failed = 1;
	} else if (!srv.enabled) {
		wlr_log(WLR_ERROR, "text-input-v3: current_enabled is false");
		failed = 1;
	}

	zwp_text_input_v3_destroy(ti);
	zwp_text_input_manager_v3_destroy(manager);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
