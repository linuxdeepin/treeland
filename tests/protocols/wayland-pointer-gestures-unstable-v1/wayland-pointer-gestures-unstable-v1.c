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
#include <errno.h>

static int read_server(struct pointer_gestures_server_state *state) {
	return TEST_READ_SERVER(pointer_gestures_read_server_state, state,
		"pointer-gestures: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("pointer-gestures: connect failed\n");
		return 1;
	}

	struct client_pointer_seat pointer_seat = { 0 };
	if (!client_bind_pointer_seat(&conn, wl_seat_interface.version, &pointer_seat)) {
		TEST_ERROR("pointer-gestures: no pointer-capable wl_seat\n");
		client_disconnect(&conn);
		return 1;
	}
	struct wl_seat *seat = pointer_seat.seat;
	struct wl_pointer *pointer = pointer_seat.pointer;

	struct zwp_pointer_gestures_v1 *manager =
		client_bind(&conn, zwp_pointer_gestures_v1_interface.name, &zwp_pointer_gestures_v1_interface, zwp_pointer_gestures_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("pointer-gestures: failed to bind manager: %s\n", strerror(errno));
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_pointer_gesture_swipe_v1 *swipe =
		zwp_pointer_gestures_v1_get_swipe_gesture(manager, pointer);
	if (swipe == NULL) {
		TEST_ERROR("pointer-gestures: get_swipe_gestures returned NULL\n");
		zwp_pointer_gestures_v1_destroy(manager);
		wl_pointer_destroy(pointer);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_pointer_gesture_pinch_v1 *pinch =
		zwp_pointer_gestures_v1_get_pinch_gesture(manager, pointer);
	if (pinch == NULL) {
		TEST_ERROR("pointer-gestures: get_pinch_gestures returned NULL\n");
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
		TEST_ERROR("pointer-gestures: wlr_pointer_gestures_v1 handle not found\n");
		failed = 1;
	} else if (srv.swipes < 1) {
		TEST_ERROR("pointer-gestures: no swipe gesture resources in production\n");
		failed = 1;
	} else if (srv.pinches < 1) {
		TEST_ERROR("pointer-gestures: no pinch gesture resources in production\n");
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
