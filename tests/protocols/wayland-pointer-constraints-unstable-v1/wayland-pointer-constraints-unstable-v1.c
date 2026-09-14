/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client obtains a wl_pointer from the
 * pointer-capable seat, creates a wl_surface, and requests a locked pointer
 * constraint.  The test then reads back the production WPointerConstraintsV1's
 * captured constraint type via the newConstraint signal, verifying the type is
 * Locked, proving the lock_pointer request reached the real compositor
 * constraint pipeline.
 */

#include "wayland-pointer-constraints-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

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

static int g_locked_unlocked = 0;

static void locked_unlocked(void *data, struct zwp_locked_pointer_v1 *locked) {
	(void)locked;
	*(int *)data = 1;
}

static const struct zwp_locked_pointer_v1_listener locked_listener = {
	.unlocked = locked_unlocked,
};


static int read_server(struct pointer_constraints_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(pointer_constraints_read_server_state, state) == 0) {
		TEST_ERROR("pointer-constraints: failed to read server state\n");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("pointer-constraints: connect failed\n");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		TEST_ERROR("pointer-constraints: no wl_seat global\n");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	if (!(g_caps & 0x1 /* WL_SEAT_CAPABILITY_POINTER */)) {
		TEST_ERROR("pointer-constraints: seat has no pointer capability\n");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	struct wl_pointer *pointer = wl_seat_get_pointer(seat);
	if (pointer == NULL) {
		TEST_ERROR("pointer-constraints: wl_seat_get_pointer returned NULL\n");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct wl_compositor *compositor =
		client_bind(&conn, wl_compositor_interface.name, &wl_compositor_interface, 4);
	if (compositor == NULL) {
		TEST_ERROR("pointer-constraints: no wl_compositor global\n");
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	struct wl_surface *surface = wl_compositor_create_surface(compositor);
	if (surface == NULL) {
		TEST_ERROR("pointer-constraints: create_surface returned NULL\n");
		wl_compositor_destroy(compositor);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	wl_surface_commit(surface);

	struct zwp_pointer_constraints_v1 *manager =
		client_bind(&conn, zwp_pointer_constraints_v1_interface.name, &zwp_pointer_constraints_v1_interface, 1);
	if (manager == NULL) {
		TEST_ERROR("pointer-constraints: failed to bind manager: %s\n", strerror(errno));
		wl_surface_destroy(surface);
		wl_compositor_destroy(compositor);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_locked_pointer_v1 *locked = zwp_pointer_constraints_v1_lock_pointer(
		manager, surface, pointer, NULL, 1 /* ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT */);
	if (locked == NULL) {
		TEST_ERROR("pointer-constraints: lock_pointer returned NULL\n");
		zwp_pointer_constraints_v1_destroy(manager);
		wl_surface_destroy(surface);
		wl_compositor_destroy(compositor);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	zwp_locked_pointer_v1_add_listener(locked, &locked_listener, &g_locked_unlocked);
	wl_display_roundtrip(conn.display);

	int failed = 0;
	if (g_locked_unlocked) {
		TEST_ERROR("pointer-constraints: locked pointer was immediately unlocked\n");
		failed = 1;
	}

	/* E-level: the production constraint must have type Locked (0). */
	struct pointer_constraints_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		TEST_ERROR("pointer-constraints: no constraint captured\n");
		failed = 1;
	} else if (srv.constraint_type != POINTER_CONSTRAINT_TYPE_LOCKED) {
		TEST_ERROR("pointer-constraints: expected type Locked(0), got %d\n",
			srv.constraint_type);
		failed = 1;
	}

	zwp_locked_pointer_v1_destroy(locked);
	zwp_pointer_constraints_v1_destroy(manager);
	wl_surface_destroy(surface);
	wl_compositor_destroy(compositor);
	wl_pointer_destroy(pointer);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
