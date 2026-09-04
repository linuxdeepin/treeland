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
#include <wlr/util/log.h>

static const char *const TEST_TAG = "treeland-tag-test";

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "xdg-toplevel-tag: connect failed");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-toplevel-tag: create_pending failed");
		client_disconnect(&conn);
		return 1;
	}

	struct xdg_toplevel_tag_manager_v1 *manager = client_bind(
		&conn, xdg_toplevel_tag_manager_v1_interface.name, &xdg_toplevel_tag_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "xdg-toplevel-tag: failed to bind manager");
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	xdg_toplevel_tag_manager_v1_set_toplevel_tag(manager, tc.toplevel, TEST_TAG);

	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-toplevel-tag: complete_map failed");
		xdg_toplevel_tag_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	/* E-level: read the real WXdgToplevelSurface's tag and compare. */
	struct xdg_toplevel_tag_server_state server;
	memset(&server, 0, sizeof(server));
	int failed = 0;
	if (invoke_on_server_thread(xdg_toplevel_tag_read_server_state, &server) == 0) {
		wlr_log(WLR_ERROR, "xdg-toplevel-tag: failed to read server state");
		failed = 1;
	} else if (!server.valid) {
		wlr_log(WLR_ERROR, "xdg-toplevel-tag: no mapped SurfaceWrapper captured");
		failed = 1;
	} else if (strcmp(server.tag, TEST_TAG) != 0) {
		wlr_log(WLR_ERROR, "xdg-toplevel-tag: real tag \"%s\" != \"%s\"", server.tag, TEST_TAG);
		failed = 1;
	}

	xdg_toplevel_tag_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
