/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the wp_viewporter global created by wlr_viewporter_create in
 * Treeland's Helper.
 *
 * Coverage level E (end-to-end): the client maps a real xdg_toplevel, creates
 * a wp_viewport for its surface, sets a destination rectangle (320×240), and
 * commits.  The test then reads back the production wlr_surface current
 * viewport state over the server bridge and verifies has_dst is set with the
 * correct destination dimensions, proving the viewport request reached the real
 * compositor surface pipeline rather than merely surviving without a protocol
 * error.
 */

#include "wayland-viewporter.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "viewporter-client-protocol.h"

#include <string.h>
#include <errno.h>

static int read_server(struct viewporter_server_state *state) {
	return TEST_READ_SERVER(viewporter_read_server_state, state,
		"viewporter: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("viewporter: connect failed\n");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 64, 64, 0xffff0000u)) {
		TEST_ERROR("viewporter: create toplevel failed\n");
		client_disconnect(&conn);
		return 1;
	}

	struct wp_viewporter *viewporter =
		client_bind(&conn, wp_viewporter_interface.name, &wp_viewporter_interface, wp_viewporter_interface.version);
	if (viewporter == NULL) {
		TEST_ERROR("viewporter: failed to bind wp_viewporter: %s\n", strerror(errno));
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct wp_viewport *viewport = wp_viewporter_get_viewport(viewporter, tc.surface);
	if (viewport == NULL) {
		TEST_ERROR("viewporter: get_viewport returned NULL\n");
		wp_viewporter_destroy(viewporter);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	int failed = 0;

	/* Apply a destination rectangle: scale the 64×64 buffer to 320×240. */
	wp_viewport_set_destination(viewport, 320, 240);
	wl_surface_commit(tc.surface);
	wl_display_roundtrip(conn.display);

	/* E-level: the real production surface viewport must have dst 320×240. */
	struct viewporter_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		TEST_ERROR("viewporter: no mapped SurfaceWrapper captured\n");
		failed = 1;
	} else if (!srv.has_dst) {
		TEST_ERROR("viewporter: viewport has_dst is false after set_destination\n");
		failed = 1;
	} else if (srv.dst_width != 320 || srv.dst_height != 240) {
		TEST_ERROR("viewporter: expected dst 320×240, got %d×%d\n",
			srv.dst_width, srv.dst_height);
		failed = 1;
	}

	wp_viewport_destroy(viewport);
	wp_viewporter_destroy(viewporter);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
