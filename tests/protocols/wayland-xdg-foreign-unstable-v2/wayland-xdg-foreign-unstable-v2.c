/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client creates two mapped xdg_toplevel
 * surfaces — a parent and a child.  It exports the parent via
 * zxdg_exporter_v2, imports the handle via zxdg_importer_v2, then calls
 * set_parent_of on the child surface.  The test reads back the real
 * production WXdgToplevelSurface::parentXdgSurface() on the child, verifying
 * it is non-null, proving the xdg-foreign-v2 set_parent_of request reached
 * the real compositor and established the parent-child relationship on the
 * real toplevel objects.
 */

#include "wayland-xdg-foreign-unstable-v2.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-foreign-unstable-v2-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

struct export_state {
	int handle_received;
	char handle[256];
};

static void exported_handle(void *data, struct zxdg_exported_v2 *exported, const char *handle) {
	(void)exported;
	struct export_state *state = data;
	state->handle_received = 1;
	if (handle != NULL) {
		strncpy(state->handle, handle, sizeof(state->handle) - 1);
		state->handle[sizeof(state->handle) - 1] = '\0';
	}
}

static const struct zxdg_exported_v2_listener exported_listener = {
	.handle = exported_handle,
};

static void imported_destroyed(void *data, struct zxdg_imported_v2 *imported) {
	(void)data;
	(void)imported;
}

static const struct zxdg_imported_v2_listener imported_listener = {
	.destroyed = imported_destroyed,
};

static int read_server(struct xdg_foreign_v2_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(xdg_foreign_v2_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "xdg-foreign-v2: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "xdg-foreign-v2: connect failed");
		return 1;
	}

	/* 1. Create and map the parent toplevel. */
	struct xdg_toplevel_client tc_parent;
	if (!xdg_toplevel_client_create(&conn, &tc_parent)) {
		wlr_log(WLR_ERROR, "xdg-foreign-v2: failed to create parent toplevel");
		client_disconnect(&conn);
		return 1;
	}

	/* 2. Create and map the child toplevel. */
	struct xdg_toplevel_client tc_child;
	if (!xdg_toplevel_client_create(&conn, &tc_child)) {
		wlr_log(WLR_ERROR, "xdg-foreign-v2: failed to create child toplevel");
		xdg_toplevel_client_destroy(&tc_parent);
		client_disconnect(&conn);
		return 1;
	}

	/* 3. Export the parent surface and obtain the handle. */
	struct export_state state;
	memset(&state, 0, sizeof(state));
	struct zxdg_exporter_v2 *exporter =
		client_bind(&conn, zxdg_exporter_v2_interface.name, &zxdg_exporter_v2_interface, 1);
	struct zxdg_importer_v2 *importer =
		client_bind(&conn, zxdg_importer_v2_interface.name, &zxdg_importer_v2_interface, 1);
	if (exporter == NULL || importer == NULL) {
		wlr_log_errno(WLR_ERROR, "xdg-foreign-v2: failed to bind exporter/importer");
		xdg_toplevel_client_destroy(&tc_child);
		xdg_toplevel_client_destroy(&tc_parent);
		client_disconnect(&conn);
		return 1;
	}

	struct zxdg_exported_v2 *exported =
		zxdg_exporter_v2_export_toplevel(exporter, tc_parent.surface);
	if (exported == NULL) {
		wlr_log(WLR_ERROR, "xdg-foreign-v2: export_toplevel returned NULL");
		xdg_toplevel_client_destroy(&tc_child);
		xdg_toplevel_client_destroy(&tc_parent);
		client_disconnect(&conn);
		return 1;
	}
	zxdg_exported_v2_add_listener(exported, &exported_listener, &state);
	wl_display_roundtrip(conn.display);

	int failed = 0;
	if (!state.handle_received || state.handle[0] == '\0') {
		wlr_log(WLR_ERROR, "xdg-foreign-v2: no handle event / empty handle");
		failed = 1;
	} else {
		/* 4. Import the handle and set the child's parent. */
		struct zxdg_imported_v2 *imported =
			zxdg_importer_v2_import_toplevel(importer, state.handle);
		if (imported == NULL) {
			wlr_log(WLR_ERROR, "xdg-foreign-v2: import_toplevel returned NULL");
			failed = 1;
		} else {
			zxdg_imported_v2_add_listener(imported, &imported_listener, NULL);
			zxdg_imported_v2_set_parent_of(imported, tc_child.surface);
			wl_display_roundtrip(conn.display);

			/* 5. E-level: read back the production parent-child relationship. */
			struct xdg_foreign_v2_server_state srv;
			if (read_server(&srv)) {
				failed = 1;
			} else if (!srv.valid) {
				wlr_log(WLR_ERROR, "xdg-foreign-v2: SurfaceWrappers not captured");
				failed = 1;
			} else if (!srv.has_parent) {
				wlr_log(WLR_ERROR, "xdg-foreign-v2: child toplevel has no parent in production");
				failed = 1;
			}

			zxdg_imported_v2_destroy(imported);
		}
	}

	zxdg_exported_v2_destroy(exported);
	xdg_toplevel_client_destroy(&tc_child);
	xdg_toplevel_client_destroy(&tc_parent);
	zxdg_importer_v2_destroy(importer);
	zxdg_exporter_v2_destroy(exporter);
	client_disconnect(&conn);
	return failed;
}
