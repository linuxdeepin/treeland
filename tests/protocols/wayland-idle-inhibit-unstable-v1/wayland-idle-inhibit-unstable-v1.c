/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end, cross-protocol): the test verifies that
 * creating a zwp_idle_inhibitor_v1 on a mapped surface actually inhibits the
 * production wlr_idle_notifier_v1.
 *
 * Step 1: Create an idle inhibitor on a mapped toplevel surface.
 * Step 2: Create an ext-idle-notify notification with 1ms timeout.  Verify
 *         `idled` does NOT fire within 500ms — proves
 *         Helper::onNewIdleInhibitor called wlr_idle_notifier_v1_set_inhibited(true),
 *         modifying production state.
 * Step 3: Destroy the inhibitor.  Create another notification.  Verify `idled`
 *         fires — proves the production notifier is no longer inhibited.
 */

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "ext-idle-notify-v1-client-protocol.h"

#include <string.h>
#include <unistd.h>
#include <poll.h>
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

struct idle_state {
	int idled;
	int resumed;
};

static void idled(void *data, struct ext_idle_notification_v1 *notif) {
	(void)notif;
	struct idle_state *state = data;
	state->idled = 1;
}

static void resumed(void *data, struct ext_idle_notification_v1 *notif) {
	(void)notif;
	struct idle_state *state = data;
	state->resumed = 1;
}

static const struct ext_idle_notification_v1_listener idle_listener = {
	.idled = idled,
	.resumed = resumed,
};

static int wait_for_idled(
		struct client_connection *conn, struct idle_state *state, int timeout_ms) {
	state->idled = 0;
	state->resumed = 0;
	int fd = wl_display_get_fd(conn->display);
	int elapsed = 0;
	while (!state->idled && elapsed < timeout_ms) {
		wl_display_dispatch_pending(conn->display);
		wl_display_flush(conn->display);
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		int remaining = timeout_ms - elapsed;
		int ret = poll(&pfd, 1, remaining < 50 ? remaining : 50);
		if (ret > 0)
			wl_display_dispatch(conn->display);
		elapsed += 50;
	}
	/* Final drain of any pending events. */
	wl_display_dispatch_pending(conn->display);
	return state->idled;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "idle-inhibit: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "idle-inhibit: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}
	g_caps = 0;
	wl_seat_add_listener(seat, &seat_listener, NULL);
	wl_display_roundtrip(conn.display);

	struct ext_idle_notifier_v1 *notifier =
		client_bind(&conn, ext_idle_notifier_v1_interface.name, &ext_idle_notifier_v1_interface, 1);
	if (notifier == NULL) {
		wlr_log_errno(WLR_ERROR, "idle-inhibit: failed to bind ext-idle-notify");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	/* Map a toplevel surface. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "idle-inhibit: create_pending failed");
		ext_idle_notifier_v1_destroy(notifier);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "idle-inhibit: complete_map failed");
		xdg_toplevel_client_destroy(&tc);
		ext_idle_notifier_v1_destroy(notifier);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	/* Step 1: Create an idle inhibitor on the mapped surface. */
	struct zwp_idle_inhibit_manager_v1 *inhibit_mgr = client_bind(
		&conn, zwp_idle_inhibit_manager_v1_interface.name, &zwp_idle_inhibit_manager_v1_interface, 1);
	if (inhibit_mgr == NULL) {
		wlr_log_errno(WLR_ERROR, "idle-inhibit: failed to bind manager");
		xdg_toplevel_client_destroy(&tc);
		ext_idle_notifier_v1_destroy(notifier);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct zwp_idle_inhibitor_v1 *inhibitor =
		zwp_idle_inhibit_manager_v1_create_inhibitor(inhibit_mgr, tc.surface);
	if (inhibitor == NULL) {
		wlr_log(WLR_ERROR, "idle-inhibit: create_inhibitor returned NULL");
		zwp_idle_inhibit_manager_v1_destroy(inhibit_mgr);
		xdg_toplevel_client_destroy(&tc);
		ext_idle_notifier_v1_destroy(notifier);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* Step 2: While inhibited, idle notification should NOT fire. */
	struct idle_state s1 = { 0, 0 };
	struct ext_idle_notification_v1 *n1 =
		ext_idle_notifier_v1_get_idle_notification(notifier, 1, seat);
	ext_idle_notification_v1_add_listener(n1, &idle_listener, &s1);

	if (wait_for_idled(&conn, &s1, 500)) {
		wlr_log(WLR_ERROR, "idle-inhibit: idled fired while inhibitor active!");
		failed = 1;
	}
	ext_idle_notification_v1_destroy(n1);

	if (failed) {
		zwp_idle_inhibitor_v1_destroy(inhibitor);
		zwp_idle_inhibit_manager_v1_destroy(inhibit_mgr);
		xdg_toplevel_client_destroy(&tc);
		ext_idle_notifier_v1_destroy(notifier);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	/* Step 3: Destroy inhibitor — idle notification should fire. */
	zwp_idle_inhibitor_v1_destroy(inhibitor);
	wl_display_roundtrip(conn.display);

	struct idle_state s2 = { 0, 0 };
	struct ext_idle_notification_v1 *n2 =
		ext_idle_notifier_v1_get_idle_notification(notifier, 1, seat);
	ext_idle_notification_v1_add_listener(n2, &idle_listener, &s2);

	if (!wait_for_idled(&conn, &s2, 2000)) {
		wlr_log(WLR_ERROR, "idle-inhibit: idled did not fire after destroying inhibitor");
		failed = 1;
	}
	ext_idle_notification_v1_destroy(n2);

	zwp_idle_inhibit_manager_v1_destroy(inhibit_mgr);
	xdg_toplevel_client_destroy(&tc);
	ext_idle_notifier_v1_destroy(notifier);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
