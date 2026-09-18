/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the zxdg_output_manager_v1 global that Treeland serves through its
 * WXdgOutputManager wrapper.  The test binds the first wl_output, obtains an
 * xdg_output for it, and receives logical_position / logical_size followed by
 * wl_output.done.
 *
 * Coverage level E (end-to-end): instead of asserting hard-coded numbers, the
 * client reads the *real* headless WOutput's layout geometry back over the
 * server bridge (WOutput::position() / WOutput::effectiveSize(), which read the
 * exact wlroots state WXdgOutputManager uses) and verifies the protocol events
 * match the live production output object.
 */

#include "wayland-xdg-output-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

struct xdg_output_state {
	int has_position;
	int32_t x;
	int32_t y;
	int has_size;
	int32_t width;
	int32_t height;
	int output_done;
};

static void logical_position(void *data, struct zxdg_output_v1 *output, int32_t x, int32_t y) {
	(void)output;
	struct xdg_output_state *state = data;
	state->has_position = 1;
	state->x = x;
	state->y = y;
}

static void logical_size(void *data, struct zxdg_output_v1 *output, int32_t width, int32_t height) {
	(void)output;
	struct xdg_output_state *state = data;
	state->has_size = 1;
	state->width = width;
	state->height = height;
}

static void output_done(void *data, struct wl_output *output)
{
	(void)output;
	struct xdg_output_state *state = data;
	state->output_done = 1;
}

static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
	int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make,
	const char *model, int32_t transform)
{
	(void)data;
	(void)output;
	(void)x;
	(void)y;
	(void)physical_width;
	(void)physical_height;
	(void)subpixel;
	(void)make;
	(void)model;
	(void)transform;
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width,
	int32_t height, int32_t refresh)
{
	(void)data;
	(void)output;
	(void)flags;
	(void)width;
	(void)height;
	(void)refresh;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor)
{
	(void)data;
	(void)output;
	(void)factor;
}

static void output_name(void *data, struct wl_output *output, const char *name)
{
	(void)data;
	(void)output;
	(void)name;
}

static void output_description(void *data, struct wl_output *output, const char *description)
{
	(void)data;
	(void)output;
	(void)description;
}

static void name(void *data, struct zxdg_output_v1 *output, const char *name) {
	(void)data;
	(void)output;
	(void)name;
}

static void description(void *data, struct zxdg_output_v1 *output, const char *description) {
	(void)data;
	(void)output;
	(void)description;
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = logical_position,
	.logical_size = logical_size,
	.name = name,
	.description = description,
};

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
	.name = output_name,
	.description = output_description,
};

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("xdg-output: connect failed\n");
		return 1;
	}

	struct xdg_output_state state = { 0 };
	/* The fixture adds its output after the compositor's startup output, so it
	 * is the last wl_output advertised by the registry. */
	struct wl_output *output = client_bind_last(&conn, wl_output_interface.name,
			&wl_output_interface, wl_output_interface.version);
	if (output == NULL) {
		TEST_ERROR("xdg-output: fixture wl_output global is unavailable\n");
		client_disconnect(&conn);
		return 1;
	}
	wl_output_add_listener(output, &output_listener, &state);

	struct zxdg_output_manager_v1 *manager =
		client_bind(&conn, zxdg_output_manager_v1_interface.name, &zxdg_output_manager_v1_interface, zxdg_output_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("xdg-output: failed to bind zxdg_output_manager_v1: %s\n", strerror(errno));
		wl_output_destroy(output);
		client_disconnect(&conn);
		return 1;
	}

	struct zxdg_output_v1 *xdg_output = zxdg_output_manager_v1_get_xdg_output(manager, output);
	if (xdg_output == NULL) {
		TEST_ERROR("xdg-output: get_xdg_output returned NULL\n");
		zxdg_output_manager_v1_destroy(manager);
		wl_output_destroy(output);
		client_disconnect(&conn);
		return 1;
	}
	zxdg_output_v1_add_listener(xdg_output, &xdg_output_listener, &state);

	wl_display_roundtrip(conn.display);

	/*
	 * E-level: read the real headless WOutput's geometry on the compositor
	 * thread and cross-check it against the protocol events just received.
	 */
	struct xdg_output_server_state server = { 0 };
	if (invoke_on_server_thread(xdg_output_read_server_state, &server) == 0) {
		TEST_ERROR("xdg-output: failed to read server output state\n");
		zxdg_output_v1_destroy(xdg_output);
		zxdg_output_manager_v1_destroy(manager);
		wl_output_destroy(output);
		client_disconnect(&conn);
		return 1;
	}

	int failed = 0;
	if (!state.output_done) {
		TEST_ERROR("xdg-output: no wl_output.done event received\n");
		failed = 1;
	} else if (!server.valid) {
		TEST_ERROR("xdg-output: no real WOutput found in the layout\n");
		failed = 1;
	} else if (!state.has_position || state.x != server.x || state.y != server.y) {
		TEST_ERROR("xdg-output: logical_position %d,%d != real WOutput %d,%d\n", state.x,
			state.y, server.x, server.y);
		failed = 1;
	} else if (!state.has_size || state.width != server.width || state.height != server.height) {
		TEST_ERROR("xdg-output: logical_size %dx%d != real WOutput %dx%d\n", state.width,
			state.height, server.width, server.height);
		failed = 1;
	}

	zxdg_output_v1_destroy(xdg_output);
	zxdg_output_manager_v1_destroy(manager);
	wl_output_destroy(output);
	client_disconnect(&conn);
	return failed;
}
