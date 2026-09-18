/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client requests an activation token,
 * commits it, receives token.done with a non-empty token string, then maps a
 * toplevel and calls xdg_activation_v1.activate with the token.  The test
 * reads back the production ActivationManagerInterfaceV1's activateRequested
 * signal, verifying the disposition is Attention (1), proving the activation
 * request reached the real compositor activation pipeline.
 */

#include "wayland-xdg-activation-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "xdg-activation-v1-client-protocol.h"

#include <string.h>
#include <errno.h>

/* Disposition values from ActivationManagerInterfaceV1::Disposition enum.
 * The activated surface is not the focus surface, so the compositor reports
 * Attention (1) rather than Active (2) or Success (0). */
enum { DISPOSITION_INVALID = 0, DISPOSITION_ATTENTION = 1, DISPOSITION_ACTIVE = 2 };

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
	return TEST_READ_SERVER(xdg_activation_read_server_state, state,
		"xdg-activation: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("xdg-activation: connect failed\n");
		return 1;
	}

	struct xdg_activation_v1 *activation =
		client_bind(&conn, xdg_activation_v1_interface.name, &xdg_activation_v1_interface, xdg_activation_v1_interface.version);
	if (activation == NULL) {
		TEST_ERROR("xdg-activation: failed to bind: %s\n", strerror(errno));
		client_disconnect(&conn);
		return 1;
	}

	struct xdg_activation_token_v1 *token_req = xdg_activation_v1_get_activation_token(activation);
	if (token_req == NULL) {
		TEST_ERROR("xdg-activation: get_activation_token returned NULL\n");
		client_disconnect(&conn);
		return 1;
	}

	struct activation_state state = { 0 };
	xdg_activation_token_v1_add_listener(token_req, &token_listener, &state);
	xdg_activation_token_v1_commit(token_req);
	wl_display_roundtrip(conn.display);

	if (!state.done || strlen(state.token) == 0) {
		TEST_ERROR("xdg-activation: no token received\n");
		xdg_activation_token_v1_destroy(token_req);
		xdg_activation_v1_destroy(activation);
		client_disconnect(&conn);
		return 1;
	}

	/* Map a toplevel to activate. */
	struct xdg_toplevel_client tc;
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		TEST_ERROR("xdg-activation: create_pending failed\n");
		xdg_activation_token_v1_destroy(token_req);
		xdg_activation_v1_destroy(activation);
		client_disconnect(&conn);
		return 1;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		TEST_ERROR("xdg-activation: complete_map failed\n");
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
		TEST_ERROR("xdg-activation: no activateRequested captured\n");
		failed = 1;
	} else if (!srv.surface_captured) {
		TEST_ERROR("xdg-activation: no surface captured\n");
		failed = 1;
	} else if (srv.disposition != DISPOSITION_ATTENTION) {
		TEST_ERROR("xdg-activation: expected disposition Attention(%d), got %d\n",
			DISPOSITION_ATTENTION, srv.disposition);
		failed = 1;
	}

	xdg_toplevel_client_destroy(&tc);
	xdg_activation_token_v1_destroy(token_req);
	xdg_activation_v1_destroy(activation);
	client_disconnect(&conn);
	return failed;
}
