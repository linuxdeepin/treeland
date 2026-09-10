/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client binds ext_foreign_toplevel_list_v1,
 * maps a real xdg_toplevel, and receives the `toplevel` event from the
 * compositor.  The test then reads back the production
 * WExtForeignToplevelListV1's toplevels list length, verifying it is at least
 * 1, proving the toplevel was registered in the real compositor
 * foreign-toplevel-list pipeline.
 */

#include "wayland-ext-foreign-toplevel-list-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

struct ftl_state {
	int toplevel_count;
};

static void toplevel(void *data, struct ext_foreign_toplevel_list_v1 *list,
		struct ext_foreign_toplevel_handle_v1 *handle) {
	(void)list;
	(void)handle;
	struct ftl_state *state = data;
	state->toplevel_count++;
}

static void finished(void *data, struct ext_foreign_toplevel_list_v1 *list) {
	(void)data;
	(void)list;
}

static const struct ext_foreign_toplevel_list_v1_listener list_listener = {
	.toplevel = toplevel,
	.finished = finished,
};

static int read_server(struct foreign_toplevel_list_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(foreign_toplevel_list_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "ext-foreign-toplevel-list: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "ext-foreign-toplevel-list: connect failed");
		return 1;
	}

	struct ext_foreign_toplevel_list_v1 *list = client_bind(
		&conn, ext_foreign_toplevel_list_v1_interface.name, &ext_foreign_toplevel_list_v1_interface, 1);
	if (list == NULL) {
		wlr_log_errno(WLR_ERROR, "ext-foreign-toplevel-list: failed to bind");
		client_disconnect(&conn);
		return 1;
	}

	struct ftl_state state;
	memset(&state, 0, sizeof(state));
	ext_foreign_toplevel_list_v1_add_listener(list, &list_listener, &state);

	/* Map a real toplevel so the compositor registers it in the list. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "ext-foreign-toplevel-list: create_pending failed");
		ext_foreign_toplevel_list_v1_destroy(list);
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "ext-foreign-toplevel-list: complete_map failed");
		xdg_toplevel_client_destroy(&tc);
		ext_foreign_toplevel_list_v1_destroy(list);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* E-level: the production list must have at least 1 toplevel. */
	struct foreign_toplevel_list_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "ext-foreign-toplevel-list: manager not found");
		failed = 1;
	} else if (srv.count < 1) {
		wlr_log(WLR_ERROR, "ext-foreign-toplevel-list: expected count >= 1, got %d", srv.count);
		failed = 1;
	}

	xdg_toplevel_client_destroy(&tc);
	ext_foreign_toplevel_list_v1_destroy(list);
	client_disconnect(&conn);
	return failed;
}
