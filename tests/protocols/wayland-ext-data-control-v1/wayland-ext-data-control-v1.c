/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client binds ext_data_control_manager_v1,
 * creates a data control device, creates an offer with a mime type, and sets
 * the selection.  The test reads back the production wlr_seat's
 * selection_source field, verifying it is non-NULL, proving the data control
 * selection request reached the real compositor seat pipeline.
 */

#include "wayland-ext-data-control-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "ext-data-control-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

struct dc_state {
	int selection_count;
	int primary_selection_count;
};

static void data_offer(void *data, struct ext_data_control_device_v1 *device,
		struct ext_data_control_offer_v1 *offer) {
	(void)data;
	(void)device;
	(void)offer;
}

static void selection(void *data, struct ext_data_control_device_v1 *device,
		struct ext_data_control_offer_v1 *offer) {
	(void)device;
	struct dc_state *state = data;
	state->selection_count++;
	if (offer != NULL)
		ext_data_control_offer_v1_destroy(offer);
}

static void primary_selection(void *data, struct ext_data_control_device_v1 *device,
		struct ext_data_control_offer_v1 *offer) {
	(void)device;
	struct dc_state *state = data;
	state->primary_selection_count++;
	if (offer != NULL)
		ext_data_control_offer_v1_destroy(offer);
}

static void finished(void *data, struct ext_data_control_device_v1 *device) {
	(void)data;
	(void)device;
}

static const struct ext_data_control_device_v1_listener device_listener = {
	.data_offer = data_offer,
	.selection = selection,
	.finished = finished,
	.primary_selection = primary_selection,
};

static int read_server(struct ext_data_control_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(ext_data_control_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "ext-data-control: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "ext-data-control: connect failed");
		return 1;
	}

	struct wl_seat *seat = client_bind(&conn, wl_seat_interface.name, &wl_seat_interface, 7);
	if (seat == NULL) {
		wlr_log(WLR_ERROR, "ext-data-control: no wl_seat global");
		client_disconnect(&conn);
		return 1;
	}

	struct ext_data_control_manager_v1 *manager = client_bind(
		&conn, ext_data_control_manager_v1_interface.name, &ext_data_control_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "ext-data-control: failed to bind manager");
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}

	struct dc_state state;
	memset(&state, 0, sizeof(state));
	struct ext_data_control_device_v1 *device =
		ext_data_control_manager_v1_get_data_device(manager, seat);
	if (device == NULL) {
		wlr_log(WLR_ERROR, "ext-data-control: get_data_device returned NULL");
		ext_data_control_manager_v1_destroy(manager);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	ext_data_control_device_v1_add_listener(device, &device_listener, &state);

	/* Receive initial selection event (NULL). */
	wl_display_roundtrip(conn.display);

	/* Create a data source and set it as the selection. */
	struct ext_data_control_source_v1 *source =
		ext_data_control_manager_v1_create_data_source(manager);
	if (source == NULL) {
		wlr_log(WLR_ERROR, "ext-data-control: create_data_source returned NULL");
		ext_data_control_device_v1_destroy(device);
		ext_data_control_manager_v1_destroy(manager);
		wl_seat_destroy(seat);
		client_disconnect(&conn);
		return 1;
	}
	ext_data_control_source_v1_offer(source, "text/plain");
	ext_data_control_device_v1_set_selection(device, source);

	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production seat must have selection_source set. */
	struct ext_data_control_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "ext-data-control: seat not found");
		failed = 1;
	} else if (!srv.has_source) {
		wlr_log(WLR_ERROR, "ext-data-control: selection_source is NULL");
		failed = 1;
	}

	ext_data_control_source_v1_destroy(source);
	ext_data_control_device_v1_destroy(device);
	ext_data_control_manager_v1_destroy(manager);
	wl_seat_destroy(seat);
	client_disconnect(&conn);
	return failed;
}
