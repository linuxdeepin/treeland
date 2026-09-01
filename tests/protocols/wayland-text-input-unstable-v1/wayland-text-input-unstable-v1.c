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

static int read_server(struct text_input_v1_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(text_input_v1_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "text-input-v1: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "text-input-v1: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "text-input-v1: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	/* Map a toplevel so we have a surface to activate text input on. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "text-input-v1: create_pending failed");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "text-input-v1: complete_map failed");
		xdg_toplevel_client_destroy(&tc);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	struct zwp_text_input_manager_v1 *manager =
		client_bind(&conn, zwp_text_input_manager_v1_interface.name, &zwp_text_input_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "text-input-v1: failed to bind manager");
		xdg_toplevel_client_destroy(&tc);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_text_input_v1 *ti = zwp_text_input_manager_v1_create_text_input(manager);
	if (ti == NULL) {
		wlr_log(WLR_ERROR, "text-input-v1: create_text_input returned NULL");
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
		wlr_log(WLR_ERROR, "text-input-v1: manager not found");
		failed = 1;
	} else if (!srv.activated) {
		wlr_log(WLR_ERROR, "text-input-v1: activate signal not emitted");
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
