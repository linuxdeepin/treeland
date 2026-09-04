/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client obtains a wl_pointer from the
 * pointer-capable seat, binds zwp_pointer_gestures_v1, and creates swipe +
 * pinch gesture resources.  The test then reads back the real production
 * wlr_pointer_gestures_v1::swipes and ::pinches wl_list lengths, verifying
 * both are non-zero, proving the gesture resources were registered in the
 * real compositor's gesture manager rather than merely surviving without a
 * protocol error.
 */

#include "wayland-pointer-gestures-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "pointer-gestures-unstable-v1-client-protocol.h"

#include <string.h>
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

static int read_server(struct pointer_gestures_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(pointer_gestures_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "pointer-gestures: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "pointer-gestures: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "pointer-gestures: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	if (!(g_caps & 0x1)) {
		wlr_log(WLR_ERROR, "pointer-gestures: seat has no pointer capability");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	struct wl_pointer *pointer = wl_seat_get_pointer(seat);
	if (pointer == NULL) {
		wlr_log(WLR_ERROR, "pointer-gestures: wl_seat_get_pointer returned NULL");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_pointer_gestures_v1 *manager =
		client_bind(&conn, zwp_pointer_gestures_v1_interface.name, &zwp_pointer_gestures_v1_interface, 3);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "pointer-gestures: failed to bind manager");
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_pointer_gesture_swipe_v1 *swipe =
		zwp_pointer_gestures_v1_get_swipe_gesture(manager, pointer);
	if (swipe == NULL) {
		wlr_log(WLR_ERROR, "pointer-gestures: get_swipe_gestures returned NULL");
		zwp_pointer_gestures_v1_destroy(manager);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_pointer_gesture_pinch_v1 *pinch =
		zwp_pointer_gestures_v1_get_pinch_gesture(manager, pointer);
	if (pinch == NULL) {
		wlr_log(WLR_ERROR, "pointer-gestures: get_pinch_gestures returned NULL");
		zwp_pointer_gesture_swipe_v1_destroy(swipe);
		zwp_pointer_gestures_v1_destroy(manager);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production wlr_pointer_gestures_v1 must have non-empty
	 * swipes and pinches lists, proving the gesture resources were registered
	 * in the real compositor gesture manager. */
	struct pointer_gestures_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "pointer-gestures: wlr_pointer_gestures_v1 handle not found");
		failed = 1;
	} else if (srv.swipes < 1) {
		wlr_log(WLR_ERROR, "pointer-gestures: no swipe gesture resources in production");
		failed = 1;
	} else if (srv.pinches < 1) {
		wlr_log(WLR_ERROR, "pointer-gestures: no pinch gesture resources in production");
		failed = 1;
	}

	zwp_pointer_gesture_swipe_v1_destroy(swipe);
	zwp_pointer_gesture_pinch_v1_destroy(pinch);
	zwp_pointer_gestures_v1_destroy(manager);
	wl_pointer_destroy(pointer);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
