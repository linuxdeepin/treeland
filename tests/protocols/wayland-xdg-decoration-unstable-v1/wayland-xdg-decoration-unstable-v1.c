/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client maps a real xdg_toplevel, attaches
 * a zxdg_toplevel_decoration_v1, requests CLIENT_SIDE mode, and commits.
 * The test then reads back the production WXdgDecorationManager's effective
 * mode for the surface via modeBySurface(), verifying the decoration request
 * reached the real compositor decoration pipeline rather than merely surviving
 * without a protocol error.
 */

#include "wayland-xdg-decoration-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

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
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(xdg_decoration_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "xdg-decoration: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "xdg-decoration: connect failed");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-decoration: create_pending failed");
		client_disconnect(&conn);
		return 1;
	}

	struct zxdg_decoration_manager_v1 *manager =
		client_bind(&conn, zxdg_decoration_manager_v1_interface.name, &zxdg_decoration_manager_v1_interface, 2);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "xdg-decoration: failed to bind manager");
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct deco_state state;
	memset(&state, 0, sizeof(state));
	struct zxdg_toplevel_decoration_v1 *deco =
		zxdg_decoration_manager_v1_get_toplevel_decoration(manager, tc.toplevel);
	if (deco == NULL) {
		wlr_log(WLR_ERROR, "xdg-decoration: get_toplevel_decoration returned NULL");
		xdg_toplevel_client_destroy(&tc);
		zxdg_decoration_manager_v1_destroy(manager);
		client_disconnect(&conn);
		return 1;
	}
	zxdg_toplevel_decoration_v1_add_listener(deco, &deco_listener, &state);
	zxdg_toplevel_decoration_v1_set_mode(deco, 1 /* CLIENT_SIDE */);

	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-decoration: complete_map failed");
		zxdg_toplevel_decoration_v1_destroy(deco);
		zxdg_decoration_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production decoration manager must report CLIENT_SIDE (2). */
	struct xdg_decoration_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "xdg-decoration: no mapped SurfaceWrapper captured");
		failed = 1;
	} else if (srv.mode != 2 /* Client */) {
		wlr_log(WLR_ERROR, "xdg-decoration: expected mode Client(2), got %d", srv.mode);
		failed = 1;
	}

	zxdg_toplevel_decoration_v1_destroy(deco);
	zxdg_decoration_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
