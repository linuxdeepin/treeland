/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client maps a real xdg_toplevel, attaches
 * a zxdg_toplevel_decoration_v1, requests each mode, and commits. The test
 * reads back the production decoration mode and verifies that software-rendered
 * server-side decorations keep a square frame without a client input mask.
 */

#include "wayland-xdg-decoration-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

struct deco_state {
	int configure_count;
	uint32_t mode;
};

static void deco_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco, uint32_t mode) {
	(void)deco;
	struct deco_state *state = data;
	state->configure_count++;
	state->mode = mode;
}

static const struct zxdg_toplevel_decoration_v1_listener deco_listener = {
	.configure = deco_configure,
};

static int read_server(struct xdg_decoration_server_state *state) {
	return TEST_READ_SERVER(xdg_decoration_read_server_state, state,
		"xdg-decoration: failed to read server state");
}

static int test_decoration_mode(const char *socket_name, uint32_t mode) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("xdg-decoration: connect failed\n");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		TEST_ERROR("xdg-decoration: create_pending failed\n");
		client_disconnect(&conn);
		return 1;
	}

	struct zxdg_decoration_manager_v1 *manager =
		client_bind(&conn, zxdg_decoration_manager_v1_interface.name, &zxdg_decoration_manager_v1_interface, zxdg_decoration_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("xdg-decoration: failed to bind manager: %s\n", strerror(errno));
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct deco_state state = { 0 };
	struct zxdg_toplevel_decoration_v1 *deco =
		zxdg_decoration_manager_v1_get_toplevel_decoration(manager, tc.toplevel);
	if (deco == NULL) {
		TEST_ERROR("xdg-decoration: get_toplevel_decoration returned NULL\n");
		xdg_toplevel_client_destroy(&tc);
		zxdg_decoration_manager_v1_destroy(manager);
		client_disconnect(&conn);
		return 1;
	}
	zxdg_toplevel_decoration_v1_add_listener(deco, &deco_listener, &state);
	zxdg_toplevel_decoration_v1_set_mode(deco, mode);

	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		TEST_ERROR("xdg-decoration: complete_map failed\n");
		zxdg_toplevel_decoration_v1_destroy(deco);
		zxdg_decoration_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	int failed = 0;
	if (state.configure_count < 1 || state.mode != mode) {
		TEST_ERROR("xdg-decoration: expected configure mode %u, got %u (count=%d)\n",
			mode, state.mode, state.configure_count);
		failed = 1;
	}

	const int server_side = mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
	const int expected_mode = server_side ? 3 /* Server */ : 2 /* Client */;
	struct xdg_decoration_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		TEST_ERROR("xdg-decoration: no mapped SurfaceWrapper captured\n");
		failed = 1;
	} else if (srv.mode != expected_mode) {
		TEST_ERROR("xdg-decoration: expected mode %d, got %d\n", expected_mode, srv.mode);
		failed = 1;
	} else if (server_side
		&& (!srv.software_renderer || !srv.decorated || srv.requested_radius <= 0
			|| srv.frame_radius != 0 || srv.has_input_mask)) {
		TEST_ERROR("xdg-decoration: software frame regression "
			"(software=%d decorated=%d requested=%g frame=%g mask=%d)\n",
			srv.software_renderer, srv.decorated, srv.requested_radius,
			srv.frame_radius, srv.has_input_mask);
		failed = 1;
	}

	zxdg_toplevel_decoration_v1_destroy(deco);
	zxdg_decoration_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}

int protocol_test_run(const char *socket_name) {
	if (test_decoration_mode(socket_name, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE))
		return 1;
	return test_decoration_mode(socket_name, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}
