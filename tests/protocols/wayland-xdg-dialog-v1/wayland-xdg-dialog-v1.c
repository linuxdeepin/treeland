/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the xdg_wm_dialog_v1 global served by Treeland (WXdgDialogManagerV1).
 * The interface has no events; the client attaches a dialog object to a real
 * mapped xdg_toplevel and marks it modal.
 *
 * Coverage level E (end-to-end): the test does not merely assert the resource
 * survives without a protocol error.  It reads the production SurfaceWrapper's
 * modal flag back over the server bridge (SurfaceWrapper::modal(), which
 * Treeland flips via SurfaceWrapper::setModal() in response to the
 * WXdgDialogManagerV1::surfaceModalChanged signal fired by set_modal) and
 * verifies it transitions false -> true, proving the request reaches the real
 * compositor object.
 */

#include "wayland-xdg-dialog-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-dialog-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

static int read_modal(struct xdg_dialog_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(xdg_dialog_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "xdg-dialog: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "xdg-dialog: connect failed");
		return 1;
	}

	/*
	 * Create and map a real xdg_toplevel first so the production
	 * SurfaceWrapper exists before set_modal is processed.
	 */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-dialog: create_pending failed");
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-dialog: complete_map failed");
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	wl_display_roundtrip(conn.display);

	int failed = 0;

	/* Baseline: the freshly-mapped toplevel is not modal. */
	struct xdg_dialog_server_state before;
	if (read_modal(&before)) {
		xdg_toplevel_client_destroy(&tc);
		client_disconnect(&conn);
		return 1;
	}
	if (!before.valid) {
		wlr_log(WLR_ERROR, "xdg-dialog: no mapped SurfaceWrapper captured");
		failed = 1;
	} else if (before.modal) {
		wlr_log(WLR_ERROR, "xdg-dialog: wrapper already modal before set_modal");
		failed = 1;
	}

	struct xdg_wm_dialog_v1 *manager = NULL;
	struct xdg_dialog_v1 *dialog = NULL;
	if (!failed) {
		manager = client_bind(&conn, xdg_wm_dialog_v1_interface.name, &xdg_wm_dialog_v1_interface, 1);
		if (manager == NULL) {
			wlr_log_errno(WLR_ERROR, "xdg-dialog: failed to bind xdg_wm_dialog_v1");
			failed = 1;
		}
	}
	if (!failed) {
		dialog = xdg_wm_dialog_v1_get_xdg_dialog(manager, tc.toplevel);
		if (dialog == NULL) {
			wlr_log(WLR_ERROR, "xdg-dialog: get_xdg_dialog returned NULL");
			failed = 1;
		}
	}
	if (!failed) {
		xdg_dialog_v1_set_modal(dialog);
		wl_display_roundtrip(conn.display);

		/* E-level: the real SurfaceWrapper must now be modal. */
		struct xdg_dialog_server_state after;
		if (read_modal(&after)) {
			failed = 1;
		} else if (!after.modal) {
			wlr_log(WLR_ERROR, "xdg-dialog: SurfaceWrapper::modal() is false after set_modal");
			failed = 1;
		}
	}

	if (dialog != NULL)
		xdg_dialog_v1_destroy(dialog);
	if (manager != NULL)
		xdg_wm_dialog_v1_destroy(manager);
	xdg_toplevel_client_destroy(&tc);
	client_disconnect(&conn);
	return failed;
}
