/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the xdg_toplevel_tag_manager_v1 global served by Treeland
 * (WXdgToplevelTagManagerV1).  The interface has no events; the client tags a
 * toplevel with a string.
 *
 * Coverage level E (end-to-end): instead of only asserting the request is
 * accepted without a protocol error, the test reads the production
 * WXdgToplevelSurface's tag back over the server bridge
 * (WXdgToplevelSurface::tag(), which WXdgToplevelTagManagerV1 populates via
 * setTag()) and verifies it equals the string the client set.
 */

#include "wayland-xdg-toplevel-tag-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-toplevel-tag-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

static const char *const TEST_TAG = "treeland-tag-test";

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("xdg-toplevel-tag: connect failed\n");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		TEST_ERROR("xdg-toplevel-tag: create_pending failed\n");
		client_disconnect(&conn);
		return 1;
	}

	struct xdg_toplevel_tag_manager_v1 *manager = client_bind(
		&conn, xdg_toplevel_tag_manager_v1_interface.name, &xdg_toplevel_tag_manager_v1_interface, xdg_toplevel_tag_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("xdg-toplevel-tag: failed to bind manager: %s\n", strerror(errno));
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	xdg_toplevel_tag_manager_v1_set_toplevel_tag(manager, tc.toplevel, TEST_TAG);

	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		TEST_ERROR("xdg-toplevel-tag: complete_map failed\n");
		xdg_toplevel_tag_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	/* E-level: read the real WXdgToplevelSurface's tag and compare. */
	struct xdg_toplevel_tag_server_state server = { 0 };
	int failed = 0;
	if (invoke_on_server_thread(xdg_toplevel_tag_read_server_state, &server) == 0) {
		TEST_ERROR("xdg-toplevel-tag: failed to read server state\n");
		failed = 1;
	} else if (!server.valid) {
		TEST_ERROR("xdg-toplevel-tag: no mapped SurfaceWrapper captured\n");
		failed = 1;
	} else if (strcmp(server.tag, TEST_TAG) != 0) {
		TEST_ERROR("xdg-toplevel-tag: real tag \"%s\" != \"%s\"\n", server.tag, TEST_TAG);
		failed = 1;
	}

	xdg_toplevel_tag_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
