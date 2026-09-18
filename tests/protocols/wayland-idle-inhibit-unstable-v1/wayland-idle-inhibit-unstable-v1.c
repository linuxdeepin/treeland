/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the test verifies that creating a
 * zwp_idle_inhibitor_v1 on a mapped surface creates the corresponding
 * production wlr_idle_inhibitor_v1.
 *
 * Step 1: Create an idle inhibitor on a mapped toplevel surface.
 * Step 2: Read back the production inhibitor and verify it references the
 *         mapped surface.
 * Step 3: Destroy the inhibitor and verify the production resource is gone.
 */

#include "wayland-idle-inhibit-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("idle-inhibit: connect failed\n");
		return 1;
	}

	/* Map a toplevel surface. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		TEST_ERROR("idle-inhibit: create_pending failed\n");
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		TEST_ERROR("idle-inhibit: complete_map failed\n");
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	/* Step 1: Create an idle inhibitor on the mapped surface. */
	struct zwp_idle_inhibit_manager_v1 *inhibit_mgr = client_bind(
		&conn, zwp_idle_inhibit_manager_v1_interface.name, &zwp_idle_inhibit_manager_v1_interface, zwp_idle_inhibit_manager_v1_interface.version);
	if (inhibit_mgr == NULL) {
		TEST_ERROR("idle-inhibit: failed to bind manager: %s\n", strerror(errno));
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_idle_inhibitor_v1 *inhibitor =
		zwp_idle_inhibit_manager_v1_create_inhibitor(inhibit_mgr, tc.surface);
	if (inhibitor == NULL) {
		TEST_ERROR("idle-inhibit: create_inhibitor returned NULL\n");
		zwp_idle_inhibit_manager_v1_destroy(inhibit_mgr);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* Step 2: The production inhibitor must reference the mapped surface. */
	struct idle_inhibit_server_state state = { 0, 0 };
	if (!invoke_on_server_thread(idle_inhibit_read_server_state, &state)
			|| !state.valid || !state.surface_mapped) {
		TEST_ERROR("idle-inhibit: no mapped production inhibitor\n");
		failed = 1;
	}

	if (failed) {
		zwp_idle_inhibitor_v1_destroy(inhibitor);
		zwp_idle_inhibit_manager_v1_destroy(inhibit_mgr);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	/* Step 3: Destroy inhibitor — the production resource must be gone. */
	zwp_idle_inhibitor_v1_destroy(inhibitor);
	wl_display_roundtrip(conn.display);
	memset(&state, 0, sizeof(state));
	if (!invoke_on_server_thread(idle_inhibit_read_server_state, &state) || state.valid) {
		TEST_ERROR("idle-inhibit: production inhibitor remains after destroy\n");
		failed = 1;
	}

	zwp_idle_inhibit_manager_v1_destroy(inhibit_mgr);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
