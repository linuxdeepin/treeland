/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client creates a primary selection source
 * with a mime type, sets it on the primary selection device, and the test
 * reads back the production wlr_seat's primary_selection_source field,
 * verifying it is non-NULL, proving the primary selection request reached the
 * real compositor seat pipeline.
 *
 * The wlroots primary-selection-v1 implementation validates the serial via
 * wlr_seat_client_validate_event_serial. To obtain a valid serial, the test
 * maps a toplevel, which triggers keyboard focus → wl_keyboard::enter with a
 * server-assigned serial. That serial is then used in set_selection.
 */

#include "wayland-primary-selection-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "primary-selection-unstable-v1-client-protocol.h"

#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>

static uint32_t g_caps;
static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps) {
	(void)data;
	(void)seat;
	g_caps = caps;
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) {
	(void)data;
	(void)seat;
	(void)name;
}
static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_caps,
	.name = seat_name,
};

/* Keyboard listener to capture the enter serial. */
static uint32_t g_enter_serial;
static void kb_keymap(void *data, struct wl_keyboard *kb, uint32_t fmt, int32_t fd, uint32_t size) {
	(void)data;
	(void)kb;
	(void)fmt;
	close(fd);
	(void)size;
}
static void kb_enter(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surf,
		struct wl_array *keys) {
	(void)data;
	(void)kb;
	(void)surf;
	(void)keys;
	g_enter_serial = serial;
}
static void kb_leave(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surf) {
	(void)data;
	(void)kb;
	(void)serial;
	(void)surf;
}
static void kb_key(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t time, uint32_t key,
		uint32_t state) {
	(void)data;
	(void)kb;
	(void)serial;
	(void)time;
	(void)key;
	(void)state;
}
static void kb_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial,
		uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	(void)data;
	(void)kb;
	(void)serial;
	(void)mods_depressed;
	(void)mods_latched;
	(void)mods_locked;
	(void)group;
}
static void kb_repeat_info(void *data, struct wl_keyboard *kb, int32_t rate, int32_t delay) {
	(void)data;
	(void)kb;
	(void)rate;
	(void)delay;
}
static const struct wl_keyboard_listener kb_listener = {
	.keymap = kb_keymap,
	.enter = kb_enter,
	.leave = kb_leave,
	.key = kb_key,
	.modifiers = kb_modifiers,
	.repeat_info = kb_repeat_info,
};

static int read_server(struct primary_selection_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(primary_selection_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "primary-selection: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "primary-selection: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "primary-selection: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	/* Get a keyboard to capture the enter serial. */
	struct wl_keyboard *kb = NULL;
	if (g_caps & 2) { /* WL_SEAT_CAPABILITY_KEYBOARD */
		kb = wl_seat_get_keyboard(seat);
		if (kb != NULL) {
			wl_keyboard_add_listener(kb, &kb_listener, NULL);
		}
	}

	/* Map a toplevel so Treeland gives it keyboard focus → serial. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create(&conn, &tc)) {
		wlr_log(WLR_ERROR, "primary-selection: toplevel create failed");
		if (kb != NULL)
			wl_keyboard_destroy(kb);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	if (g_enter_serial == 0) {
		wlr_log(WLR_ERROR, "primary-selection: no keyboard enter serial");
		xdg_toplevel_client_destroy(&tc);
		if (kb != NULL)
			wl_keyboard_destroy(kb);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_primary_selection_device_manager_v1 *manager =
		client_bind(&conn, zwp_primary_selection_device_manager_v1_interface.name, &zwp_primary_selection_device_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "primary-selection: failed to bind manager");
		xdg_toplevel_client_destroy(&tc);
		if (kb != NULL)
			wl_keyboard_destroy(kb);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_primary_selection_device_v1 *device =
		zwp_primary_selection_device_manager_v1_get_device(manager, seat);
	if (device == NULL) {
		wlr_log(WLR_ERROR, "primary-selection: get_device returned NULL");
		zwp_primary_selection_device_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		if (kb != NULL)
			wl_keyboard_destroy(kb);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_primary_selection_source_v1 *source =
		zwp_primary_selection_device_manager_v1_create_source(manager);
	if (source == NULL) {
		wlr_log(WLR_ERROR, "primary-selection: create_source returned NULL");
		zwp_primary_selection_device_v1_destroy(device);
		zwp_primary_selection_device_manager_v1_destroy(manager);
		xdg_toplevel_client_destroy(&tc);
		if (kb != NULL)
			wl_keyboard_destroy(kb);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	zwp_primary_selection_source_v1_offer(source, "text/plain");
	zwp_primary_selection_device_v1_set_selection(device, source, g_enter_serial);

	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production seat must have primary_selection_source set. */
	struct primary_selection_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "primary-selection: seat not found");
		failed = 1;
	} else if (!srv.has_source) {
		wlr_log(WLR_ERROR, "primary-selection: primary_selection_source is NULL");
		failed = 1;
	}

	zwp_primary_selection_source_v1_destroy(source);
	zwp_primary_selection_device_v1_destroy(device);
	zwp_primary_selection_device_manager_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	if (kb != NULL)
		wl_keyboard_destroy(kb);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
