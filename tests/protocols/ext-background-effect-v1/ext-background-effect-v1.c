/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the ext_background_effect_manager_v1 global attached by Treeland's
 * Helper (WBackgroundEffectManagerV1).
 *
 * Coverage level E (end-to-end): the client binds the manager, checks the
 * blur capability, maps a real xdg_toplevel, attaches a background effect
 * object and drives the double-buffered blur region through
 * set/commit/remove/re-enable/destroy cycles.  After every commit the test
 * reads back the production SurfaceWrapper blur region over the server
 * bridge, proving the requests reach the real compositor surface pipeline
 * and that the region survives repeated commits (double-buffered state is
 * kept, not cleared, between commits).
 */

#include "ext-background-effect-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "ext-background-effect-v1-client-protocol.h"

#include <stdint.h>
#include <string.h>

static int read_server(struct background_effect_server_state *state) {
	return TEST_READ_SERVER(background_effect_read_server_state, state,
		"background-effect: failed to read server state");
}

/* Waits until a mapped SurfaceWrapper was captured, bounded by roundtrips. */
static int wait_server_wrapper(struct client_connection *conn,
		struct background_effect_server_state *state) {
	for (int i = 0; i < 50; ++i) {
		if (read_server(state))
			return 0;
		if (state->valid)
			return 1;
		if (wl_display_roundtrip(conn->display) < 0)
			return 0;
	}
	return state->valid;
}

static int expect_region(struct background_effect_server_state *state,
		int x, int y, int width, int height) {
	return state->valid && state->has_region && state->blur_enabled
		&& state->x == x && state->y == y
		&& state->width == width && state->height == height;
}

static int expect_no_region(struct background_effect_server_state *state) {
	return state->valid && !state->has_region && !state->blur_enabled;
}

static void set_blur_region(struct ext_background_effect_surface_v1 *effect,
		struct wl_compositor *compositor, int x, int y, int width, int height) {
	struct wl_region *region = wl_compositor_create_region(compositor);
	wl_region_add(region, x, y, width, height);
	ext_background_effect_surface_v1_set_blur_region(effect, region);
	/* set_blur_region has copy semantics, safe to destroy immediately. */
	wl_region_destroy(region);
}

static void handle_capabilities(void *data,
		struct ext_background_effect_manager_v1 *manager, uint32_t flags) {
	(void)manager;
	*(uint32_t *)data = flags;
}

/* The manager interface has no other events in version 1. */
static const struct ext_background_effect_manager_v1_listener manager_listener = {
	.capabilities = handle_capabilities,
};

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("background-effect: connect failed\n");
		return 1;
	}

	int failed = 0;
	uint32_t capabilities = 0;
	struct background_effect_server_state srv;
	memset(&srv, 0, sizeof(srv));

	struct ext_background_effect_manager_v1 *manager =
		client_bind(&conn, ext_background_effect_manager_v1_interface.name,
			&ext_background_effect_manager_v1_interface, 1);
	if (manager == NULL) {
		TEST_ERROR("background-effect: failed to bind ext_background_effect_manager_v1\n");
		client_disconnect(&conn);
		return 1;
	}

	/* manager.bind: capabilities must arrive on bind and contain blur. */
	ext_background_effect_manager_v1_add_listener(manager, &manager_listener, &capabilities);
	if (wl_display_roundtrip(conn.display) < 0) {
		TEST_ERROR("background-effect: roundtrip after bind failed\n");
		client_disconnect(&conn);
		return 1;
	}
	if (capabilities != EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR) {
		TEST_ERROR("background-effect: expected capabilities %u, got %u\n",
			EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR, capabilities);
		failed = 1;
	}

	struct xdg_toplevel_client tc;
	if (!failed
			&& !xdg_toplevel_client_create_with_solid_buffer(&conn, &tc, 128, 128, 0xff00ff00u)) {
		TEST_ERROR("background-effect: create toplevel failed\n");
		failed = 1;
	}

	struct ext_background_effect_surface_v1 *effect = NULL;
	if (!failed) {
		effect = ext_background_effect_manager_v1_get_background_effect(manager, tc.surface);
		if (effect == NULL) {
			TEST_ERROR("background-effect: get_background_effect failed\n");
			failed = 1;
		}
	}

	/* set_blur_region + commit: the region must reach the production wrapper. */
	if (!failed) {
		set_blur_region(effect, tc.compositor, 10, 10, 40, 40);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);
		if (!wait_server_wrapper(&conn, &srv) || !expect_region(&srv, 10, 10, 40, 40)) {
			TEST_ERROR("background-effect: expected region 10,10 40x40, got valid=%d has_region=%d %d,%d %dx%d\n",
				srv.valid, srv.has_region, srv.x, srv.y, srv.width, srv.height);
			failed = 1;
		}
	}

	/* Double-buffered state: a commit without set_blur_region must keep the
	 * region (the pending region is not cleared by the move to current). */
	if (!failed) {
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);
			if (read_server(&srv) || !expect_region(&srv, 10, 10, 40, 40)) {
			TEST_ERROR("background-effect: region lost after plain commit (got %d,%d %dx%d)\n",
				srv.x, srv.y, srv.width, srv.height);
			failed = 1;
		}
	}

	/* NULL region removes the effect on the next commit. */
	if (!failed) {
		ext_background_effect_surface_v1_set_blur_region(effect, NULL);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);
		if (read_server(&srv) || !expect_no_region(&srv)) {
			TEST_ERROR("background-effect: region not removed by NULL region\n");
			failed = 1;
		}
	}

	/* Re-enable with a region covering the whole surface. */
	if (!failed) {
		set_blur_region(effect, tc.compositor, 0, 0, 128, 128);
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);
		if (read_server(&srv) || !expect_region(&srv, 0, 0, 128, 128)) {
			TEST_ERROR("background-effect: region not re-enabled after NULL\n");
			failed = 1;
		}
	}

	/* Destroying the effect object removes the effect on the next commit. */
	if (!failed) {
		ext_background_effect_surface_v1_destroy(effect);
		effect = NULL;
		wl_surface_commit(tc.surface);
		wl_display_roundtrip(conn.display);
		if (read_server(&srv) || !expect_no_region(&srv)) {
			TEST_ERROR("background-effect: region not removed after effect destroy\n");
			failed = 1;
		}
	}

	/* A second get_background_effect on the same surface must raise the
	 * background_effect_exists protocol error (fatal, so run it last). */
	if (!failed) {
		effect = ext_background_effect_manager_v1_get_background_effect(manager, tc.surface);
		if (effect == NULL) {
			TEST_ERROR("background-effect: re-attach after destroy failed\n");
			failed = 1;
		} else {
			ext_background_effect_manager_v1_get_background_effect(manager, tc.surface);
			if (wl_display_roundtrip(conn.display) >= 0) {
				TEST_ERROR("background-effect: expected a protocol error for duplicate effect object\n");
				failed = 1;
			} else {
				const struct wl_interface *error_interface = NULL;
				uint32_t error_id = 0;
				uint32_t code = wl_display_get_protocol_error(conn.display,
					&error_interface, &error_id);
				if (code != EXT_BACKGROUND_EFFECT_MANAGER_V1_ERROR_BACKGROUND_EFFECT_EXISTS) {
					TEST_ERROR("background-effect: expected background_effect_exists error, got %u\n", code);
					failed = 1;
				}
			}
		}
	}

	if (effect)
		ext_background_effect_surface_v1_destroy(effect);
	ext_background_effect_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
