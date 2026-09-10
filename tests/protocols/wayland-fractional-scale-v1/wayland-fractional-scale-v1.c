/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the wp_fractional_scale_manager_v1 global served by Treeland
 * (wlr_fractional_scale_manager_v1_create).  The client maps a toplevel with a
 * solid buffer, attaches a fractional-scale object, commits, and asserts the
 * server delivers a `preferred_scale` event.
 *
 * Coverage level E (end-to-end): the test cross-checks the preferred_scale
 * event against the live WOutput scale read back over the server bridge.
 * wlroots sends preferred_scale as a numerator with denominator 120, so the
 * expected value is round(WOutput::scale() * 120).  Verifying this match
 * proves the event reflects the real production output state.
 */

#include "wayland-fractional-scale-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "fractional-scale-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

struct fs_state {
	int preferred_scale_count;
	uint32_t scale;
};

static void preferred_scale(void *data, struct wp_fractional_scale_v1 *fs, uint32_t scale) {
	(void)fs;
	struct fs_state *state = data;
	state->preferred_scale_count++;
	state->scale = scale;
}

static const struct wp_fractional_scale_v1_listener fs_listener = {
	.preferred_scale = preferred_scale,
};

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "fractional-scale: connect failed");
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 64, 64, 0xffff0000u)) {
		wlr_log(WLR_ERROR, "fractional-scale: create toplevel failed");
		client_disconnect(&conn);
		return 1;
	}

	struct wp_fractional_scale_manager_v1 *manager = client_bind(
		&conn, wp_fractional_scale_manager_v1_interface.name, &wp_fractional_scale_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "fractional-scale: failed to bind manager");
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}

	struct fs_state state;
	memset(&state, 0, sizeof(state));
	struct wp_fractional_scale_v1 *fs =
		wp_fractional_scale_manager_v1_get_fractional_scale(manager, tc.surface);
	if (fs == NULL) {
		wlr_log(WLR_ERROR, "fractional-scale: get_fractional_scale returned NULL");
		wp_fractional_scale_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wp_fractional_scale_v1_add_listener(fs, &fs_listener, &state);

	wl_surface_commit(tc.surface);
	wl_display_roundtrip(conn.display);

	int failed = 0;
	if (state.preferred_scale_count < 1) {
		wlr_log(WLR_ERROR, "fractional-scale: no preferred_scale event received");
		failed = 1;
	} else if (state.scale == 0) {
		wlr_log(WLR_ERROR, "fractional-scale: preferred_scale reported 0");
		failed = 1;
	}

	/* E-level: cross-check the preferred_scale against the live WOutput scale. */
	if (!failed) {
		struct fractional_scale_server_state srv;
		memset(&srv, 0, sizeof(srv));
		if (invoke_on_server_thread(fractional_scale_read_server_state, &srv) == 0) {
			wlr_log(WLR_ERROR, "fractional-scale: failed to read server state");
			failed = 1;
		} else if (!srv.valid) {
			wlr_log(WLR_ERROR, "fractional-scale: no WOutput found");
			failed = 1;
		} else {
			uint32_t expected = (uint32_t)(srv.scale * 120.0f + 0.5f);
			if (expected != state.scale) {
				wlr_log(WLR_ERROR,
					"fractional-scale: preferred_scale=%u but WOutput scale=%.2f"
					" (expected %u)",
					state.scale, (double)srv.scale, expected);
				failed = 1;
			}
		}
	}

	wp_fractional_scale_v1_destroy(fs);
	wp_fractional_scale_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
