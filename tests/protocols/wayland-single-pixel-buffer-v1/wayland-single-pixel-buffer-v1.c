/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the wp_single_pixel_buffer_manager_v1 global created by
 * wlr_single_pixel_buffer_manager_v1_create in Treeland's Helper.
 *
 * Coverage level E (end-to-end): the client maps a real xdg_toplevel using a
 * single-pixel RGBA buffer as its content, then reads back the production
 * wlr_surface current state over the server bridge.  The surface must be
 * mapped and its buffer dimensions must be 1×1, proving the buffer was accepted
 * by the real compositor surface pipeline rather than merely surviving without
 * a protocol error.
 */

#include "wayland-single-pixel-buffer-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "single-pixel-buffer-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

static int read_server(struct single_pixel_buffer_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(single_pixel_buffer_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "single-pixel-buffer: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "single-pixel-buffer: connect failed");
		return 1;
	}

	struct wp_single_pixel_buffer_manager_v1 *manager = client_bind(&conn,
		wp_single_pixel_buffer_manager_v1_interface.name, &wp_single_pixel_buffer_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "single-pixel-buffer: failed to bind manager");
		client_disconnect(&conn);
		return 1;
	}

	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "single-pixel-buffer: create toplevel pending failed");
		wp_single_pixel_buffer_manager_v1_destroy(manager);
		client_disconnect(&conn);
		return 1;
	}

	int failed = 0;

	/* Receive the initial configure event. */
	if (wl_display_roundtrip(conn.display) < 0 || !tc.configured) {
		wlr_log(WLR_ERROR, "single-pixel-buffer: no configure event");
		failed = 1;
	}

	/* Ack the configure so the surface enters the mapped-ready state. */
	if (!failed && !xdg_toplevel_client_ack_latest_configure(&conn, &tc)) {
		wlr_log(WLR_ERROR, "single-pixel-buffer: ack configure failed");
		failed = 1;
	}

	struct wl_buffer *buffer = NULL;
	if (!failed) {
		buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
			manager, 0xffffu, 0x0000u, 0x0000u, 0xffffu);
		if (buffer == NULL) {
			wlr_log(WLR_ERROR, "single-pixel-buffer: create_u32_rgba_buffer returned NULL");
			failed = 1;
		}
	}

	if (!failed) {
		/* Attach the 1×1 single-pixel buffer and commit to map the toplevel. */
		wl_surface_attach(tc.surface, buffer, 0, 0);
		wl_surface_damage(tc.surface, 0, 0, 1, 1);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);

		/* E-level: the real production surface must be mapped with a 1×1 buffer. */
		struct single_pixel_buffer_server_state srv;
		if (read_server(&srv)) {
			failed = 1;
		} else if (!srv.valid) {
			wlr_log(WLR_ERROR, "single-pixel-buffer: no mapped SurfaceWrapper captured");
			failed = 1;
		} else if (!srv.mapped) {
			wlr_log(WLR_ERROR, "single-pixel-buffer: WSurface not mapped");
			failed = 1;
		} else if (srv.buffer_width != 1 || srv.buffer_height != 1) {
			wlr_log(WLR_ERROR, "single-pixel-buffer: expected 1×1 buffer, got %d×%d", srv.buffer_width, srv.buffer_height);
			failed = 1;
		}
	}

	if (buffer != NULL)
		wl_buffer_destroy(buffer);
	xdg_toplevel_client_destroy(&tc);
	wp_single_pixel_buffer_manager_v1_destroy(manager);
	client_disconnect(&conn);
	return failed;
}
