/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client requests an activation token,
 * commits it, receives token.done with a non-empty token string, then maps a
 * toplevel and calls xdg_activation_v1.activate with the token.  The test
 * reads back the production ActivationManagerInterfaceV1's activateRequested
 * signal, verifying the disposition is non-Invalid, proving the activation
 * request reached the real compositor activation pipeline.
 */

#include "wayland-xdg-activation-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-activation-v1-client-protocol.h"

#include <string.h>
#include <wlr/util/log.h>

struct activation_state {
	int done;
	char token[256];
};

static void done(void *data, struct xdg_activation_token_v1 *token, const char *token_string) {
	(void)token;
	struct activation_state *state = data;
	state->done = 1;
	if (token_string != NULL) {
		strncpy(state->token, token_string, sizeof(state->token) - 1);
		state->token[sizeof(state->token) - 1] = '\0';
	}
}

static const struct xdg_activation_token_v1_listener token_listener = {
	.done = done,
};

static int read_server(struct xdg_activation_server_state *state) {
	memset(state, 0, sizeof(*state));
	if (invoke_on_server_thread(xdg_activation_read_server_state, state) == 0) {
		wlr_log(WLR_ERROR, "xdg-activation: failed to read server state");
		return 1;
	}
	return 0;
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "xdg-activation: connect failed");
		return 1;
	}

	struct xdg_activation_v1 *activation =
		client_bind(&conn, xdg_activation_v1_interface.name, &xdg_activation_v1_interface, 1);
	if (activation == NULL) {
		wlr_log_errno(WLR_ERROR, "xdg-activation: failed to bind");
		client_disconnect(&conn);
		return 1;
	}

	struct xdg_activation_token_v1 *token_req = xdg_activation_v1_get_activation_token(activation);
	if (token_req == NULL) {
		wlr_log(WLR_ERROR, "xdg-activation: get_activation_token returned NULL");
		client_disconnect(&conn);
		return 1;
	}

	struct activation_state state;
	memset(&state, 0, sizeof(state));
	xdg_activation_token_v1_add_listener(token_req, &token_listener, &state);
	xdg_activation_token_v1_commit(token_req);
	wl_display_roundtrip(conn.display);

	if (!state.done || strlen(state.token) == 0) {
		wlr_log(WLR_ERROR, "xdg-activation: no token received");
		xdg_activation_token_v1_destroy(token_req);
		xdg_activation_v1_destroy(activation);
		client_disconnect(&conn);
		return 1;
	}

	/* Map a toplevel to activate. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-activation: create_pending failed");
		xdg_activation_token_v1_destroy(token_req);
		xdg_activation_v1_destroy(activation);
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		wlr_log(WLR_ERROR, "xdg-activation: complete_map failed");
		xdg_toplevel_client_destroy(&tc);
		xdg_activation_token_v1_destroy(token_req);
		xdg_activation_v1_destroy(activation);
		client_disconnect(&conn);
		return 1;
	}

	xdg_activation_v1_activate(activation, state.token, tc.surface);
	wl_display_roundtrip(conn.display);

	int failed = 0;
	struct xdg_activation_server_state srv;
	if (read_server(&srv)) {
		failed = 1;
	} else if (!srv.valid) {
		wlr_log(WLR_ERROR, "xdg-activation: no activateRequested captured");
		failed = 1;
	} else if (srv.disposition == 0 /* Invalid */) {
		wlr_log(WLR_ERROR, "xdg-activation: disposition is Invalid");
		failed = 1;
	}

	xdg_toplevel_client_destroy(&tc);
	xdg_activation_token_v1_destroy(token_req);
	xdg_activation_v1_destroy(activation);
	client_disconnect(&conn);
	return failed;
}
