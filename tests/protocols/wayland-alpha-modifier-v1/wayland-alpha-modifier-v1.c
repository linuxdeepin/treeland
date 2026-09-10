/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the wp_alpha_modifier_v1 global created by wlr_alpha_modifier_v1_create
 * in Treeland's Helper.
 *
 * Coverage level E (end-to-end): the client maps a real xdg_toplevel, attaches
 * an alpha-modifier surface object, sets the multiplier to 0 (fully
 * transparent) and then to UINT32_MAX (fully opaque), committing after each
 * change.  The test reads back the production wlr_alpha_modifier_surface_v1
 * state over the server bridge and verifies the multiplier transitions
 * 0.0 → 1.0, proving the request reached the real compositor surface pipeline
 * rather than merely surviving without a protocol error.
 */

#include "wayland-alpha-modifier-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "alpha-modifier-v1-client-protocol.h"

#include <string.h>
#include <stdint.h>
#include <wlr/util/log.h>

static int read_server(struct alpha_modifier_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(alpha_modifier_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "alpha-modifier: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "alpha-modifier: connect failed");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 64, 64, 0xffff0000u)) {
		wlr_log(WLR_ERROR, "alpha-modifier: create toplevel failed");
		client_disconnect(&conn);
		return 1;
	}

	struct wp_alpha_modifier_v1 *manager =
		client_bind(&conn, wp_alpha_modifier_v1_interface.name, &wp_alpha_modifier_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "alpha-modifier: failed to bind wp_alpha_modifier_v1");
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct wp_alpha_modifier_surface_v1 *am_surface =
		wp_alpha_modifier_v1_get_surface(manager, tc.surface);
	if (am_surface == NULL) {
		wlr_log(WLR_ERROR, "alpha-modifier: get_surface returned NULL");
		wp_alpha_modifier_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	int failed = 0;

	/* Set multiplier to 0 (fully transparent) and commit. */
	wp_alpha_modifier_surface_v1_set_multiplier(am_surface, 0u);
	wl_surface_commit(tc.surface);
	wl_display_roundtrip(conn.display);

	/* E-level: the production multiplier must be 0.0. */
	struct alpha_modifier_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid || !srv.has_modifier) {
		wlr_log(WLR_ERROR, "alpha-modifier: no alpha modifier state on surface");
		failed = 1;
	} else if (srv.multiplier != 0.0) {
		wlr_log(WLR_ERROR, "alpha-modifier: expected multiplier 0.0, got %f", srv.multiplier);
		failed = 1;
	}

	/* Set multiplier to UINT32_MAX (fully opaque) and commit. */
	if (!failed) {
		wp_alpha_modifier_surface_v1_set_multiplier(am_surface, 0xffffffffu);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);

		if (read_server(&srv)) {
			failed = 1;
		} else if (srv.multiplier != 1.0) {
			wlr_log(WLR_ERROR, "alpha-modifier: expected multiplier 1.0, got %f", srv.multiplier);
			failed = 1;
		}
	}

	wp_alpha_modifier_surface_v1_destroy(am_surface);
	wp_alpha_modifier_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
