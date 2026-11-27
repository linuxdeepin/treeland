/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the wp_alpha_modifier_v1 global created by wlr_alpha_modifier_v1_create
 * in Treeland's WaylandServer.
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
#include <errno.h>

static int read_server(struct alpha_modifier_server_state *state) {
	return TEST_READ_SERVER(alpha_modifier_read_server_state, state,
		"alpha-modifier: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("alpha-modifier: connect failed\n");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 64, 64, 0xffff0000u)) {
		TEST_ERROR("alpha-modifier: create toplevel failed\n");
		client_disconnect(&conn);
		return 1;
	}

	struct wp_alpha_modifier_v1 *manager =
		client_bind(&conn, wp_alpha_modifier_v1_interface.name, &wp_alpha_modifier_v1_interface, wp_alpha_modifier_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("alpha-modifier: failed to bind wp_alpha_modifier_v1: %s\n", strerror(errno));
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct wp_alpha_modifier_surface_v1 *am_surface =
		wp_alpha_modifier_v1_get_surface(manager, tc.surface);
	if (am_surface == NULL) {
		TEST_ERROR("alpha-modifier: get_surface returned NULL\n");
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
		TEST_ERROR("alpha-modifier: no alpha modifier state on surface\n");
		failed = 1;
	} else if (srv.multiplier != 0.0) {
		TEST_ERROR("alpha-modifier: expected multiplier 0.0, got %f\n", srv.multiplier);
		failed = 1;
	}

	/* Set multiplier to ~0.5 and verify DConfig windowTransparencyEnabled toggles
	 * SurfaceContent opacity without squaring SurfaceItemContent opacity. */
	if (!failed) {
		wp_alpha_modifier_surface_v1_set_multiplier(am_surface, 0x80000000u);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);

		if (read_server(&srv)) {
			failed = 1;
		} else if (srv.multiplier < 0.49 || srv.multiplier > 0.51) {
			TEST_ERROR("alpha-modifier: expected multiplier ~0.5, got %f\n", srv.multiplier);
			failed = 1;
		} else if (!srv.has_content_item || !srv.has_item_content) {
			TEST_ERROR("alpha-modifier: missing content_item=%d or item_content=%d\n",
				srv.has_content_item, srv.has_item_content);
			failed = 1;
		} else if (srv.content_opacity < 0.49 || srv.content_opacity > 0.51
				|| srv.item_content_opacity != 1.0) {
			TEST_ERROR("alpha-modifier: unexpected content opacity=%f item_content_opacity=%f\n",
				srv.content_opacity, srv.item_content_opacity);
			failed = 1;
		}
	}
	if (!failed) {
		int enabled = 0;
		if (!invoke_on_server_thread(alpha_modifier_set_transparency_enabled, &enabled)
				|| read_server(&srv)) {
			failed = 1;
		} else if (!srv.has_content_item || srv.content_opacity != 1.0) {
			TEST_ERROR("alpha-modifier: DConfig disabled transparency (has_content_item=%d content_opacity=%f)\n",
				srv.has_content_item, srv.content_opacity);
			failed = 1;
		}
	}
	if (!failed) {
		int enabled = 1;
		if (!invoke_on_server_thread(alpha_modifier_set_transparency_enabled, &enabled)
				|| read_server(&srv)) {
			failed = 1;
		} else if (!srv.has_content_item || srv.content_opacity < 0.49 || srv.content_opacity > 0.51) {
			TEST_ERROR("alpha-modifier: DConfig re-enabled transparency (has_content_item=%d content_opacity=%f)\n",
				srv.has_content_item, srv.content_opacity);
			failed = 1;
		}
	}
	if (!failed) {
		if (!invoke_on_server_thread(alpha_modifier_set_effect_level, (void *)"low")
				|| read_server(&srv)) {
			failed = 1;
		} else if (!srv.has_content_item || srv.content_opacity != 1.0) {
			TEST_ERROR("alpha-modifier: effectLevel=low did not disable transparency (has_content_item=%d content_opacity=%f)\n",
				srv.has_content_item, srv.content_opacity);
			failed = 1;
		}
	}
	if (!failed) {
		if (!invoke_on_server_thread(alpha_modifier_set_effect_level, (void *)"medium")
				|| read_server(&srv)) {
			failed = 1;
		} else if (!srv.has_content_item || srv.content_opacity < 0.49 || srv.content_opacity > 0.51) {
			TEST_ERROR("alpha-modifier: effectLevel=medium did not re-enable transparency (has_content_item=%d content_opacity=%f)\n",
				srv.has_content_item, srv.content_opacity);
			failed = 1;
		}
	}

	/* Set multiplier to UINT32_MAX (fully opaque) and commit. */
	if (!failed) {
		wp_alpha_modifier_surface_v1_set_multiplier(am_surface, 0xffffffffu);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);

		if (read_server(&srv)) {
			failed = 1;
		} else if (srv.multiplier != 1.0) {
			TEST_ERROR("alpha-modifier: expected multiplier 1.0, got %f\n", srv.multiplier);
			failed = 1;
		}
	}

	wp_alpha_modifier_surface_v1_destroy(am_surface);
	wp_alpha_modifier_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
