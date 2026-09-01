/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client creates an abstract Unix listening
 * socket + a close fd, commits a security context with app_id="test-app", then
 * connects a second Wayland client through the listening socket.  The test
 * reads back the production wlr_security_context_manager_v1's commit event,
 * verifying the app_id matches, proving the security context commit reached the
 * real compositor security-context pipeline.
 */

#include "wayland-security-context-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "security-context-v1-client-protocol.h"

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <wlr/util/log.h>

struct sec_state {
	int global_count;
	int has_compositor;
};

static void sec_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version) {
	(void)registry;
	(void)name;
	(void)version;
	struct sec_state *state = data;
	state->global_count++;
	if (strcmp(interface, "wl_compositor") == 0)
		state->has_compositor = 1;
}

static void sec_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener sec_registry_listener = {
	.global = sec_global,
	.global_remove = sec_global_remove,
};

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "security-context: connect failed");
		return 1;
	}

	/* 1. Abstract Unix listening socket. */
	int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		wlr_log(WLR_ERROR, "security-context: socket() failed");
		client_disconnect(&conn);
		return 1;
	}
	char name[64];
	snprintf(name, sizeof(name), "treeland-sec-ctx-%d", (int)getpid());
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0'; /* abstract namespace marker */
	strncpy(addr.sun_path + 1, name, sizeof(addr.sun_path) - 2);
	socklen_t addrlen = (socklen_t)(sizeof(sa_family_t) + 1 + strlen(name));
	if (bind(listen_fd, (struct sockaddr *)&addr, addrlen) < 0 || listen(listen_fd, 1) < 0) {
		wlr_log(WLR_ERROR, "security-context: bind/listen failed");
		close(listen_fd);
		client_disconnect(&conn);
		return 1;
	}

	/* 2. close fd (socketpair); the compositor watches one end. */
	int close_fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, close_fds) < 0) {
		wlr_log(WLR_ERROR, "security-context: socketpair failed");
		close(listen_fd);
		client_disconnect(&conn);
		return 1;
	}

	/* 3. Bind manager, create the security context, commit. */
	struct wp_security_context_manager_v1 *manager = client_bind(
		&conn, wp_security_context_manager_v1_interface.name, &wp_security_context_manager_v1_interface, 1);
	if (manager == NULL) {
		wlr_log_errno(WLR_ERROR, "security-context: failed to bind manager");
		close(close_fds[0]);
		close(close_fds[1]);
		close(listen_fd);
		client_disconnect(&conn);
		return 1;
	}

	struct wp_security_context_v1 *ctx =
		wp_security_context_manager_v1_create_listener(manager, listen_fd, close_fds[0]);
	if (ctx == NULL) {
		wlr_log(WLR_ERROR, "security-context: create_listener returned NULL");
		close(close_fds[0]);
		close(close_fds[1]);
		close(listen_fd);
		client_disconnect(&conn);
		return 1;
	}
	wp_security_context_v1_set_app_id(ctx, "test-app");
	wp_security_context_v1_commit(ctx);
	/* Let the compositor add listen_fd to its event loop. */
	wl_display_roundtrip(conn.display);

	/* E-level: read back the production commit event state. */
	struct security_context_server_state srv;
	memset(&srv, 0, sizeof(srv));
	invoke_on_server_thread(security_context_read_server_state, &srv);

	/* 4. Connect a second client through the listening socket. */
	int conn_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (conn_fd < 0 || connect(conn_fd, (struct sockaddr *)&addr, addrlen) < 0) {
		wlr_log(WLR_ERROR, "security-context: second connect failed");
		if (conn_fd >= 0)
			close(conn_fd);
		wp_security_context_v1_destroy(ctx);
		wp_security_context_manager_v1_destroy(manager);
		close(close_fds[0]);
		close(close_fds[1]);
		close(listen_fd);
		client_disconnect(&conn);
		return 1;
	}

	/* The compositor must accept() on its next loop iteration. */
	wl_display_roundtrip(conn.display);

	struct wl_display *sec_display = wl_display_connect_to_fd(conn_fd);
	if (sec_display == NULL) {
		wlr_log(WLR_ERROR, "security-context: wl_display_connect_to_fd failed");
		wp_security_context_v1_destroy(ctx);
		wp_security_context_manager_v1_destroy(manager);
		close(close_fds[0]);
		close(close_fds[1]);
		close(listen_fd);
		client_disconnect(&conn);
		return 1;
	}

	struct sec_state state;
	memset(&state, 0, sizeof(state));
	struct wl_registry *registry = wl_display_get_registry(sec_display);
	wl_registry_add_listener(registry, &sec_registry_listener, &state);
	wl_display_roundtrip(sec_display);

	int failed = 0;
	if (state.global_count < 1) {
		wlr_log(WLR_ERROR, "security-context: no globals on second client");
		failed = 1;
	} else if (!state.has_compositor) {
		wlr_log(WLR_ERROR, "security-context: wl_compositor not advertised");
		failed = 1;
	}

	/* E-level: verify the production commit captured the correct app_id. */
	if (!srv.valid) {
		wlr_log(WLR_ERROR, "security-context: no commit event captured");
		failed = 1;
	} else if (!srv.app_id_match) {
		wlr_log(WLR_ERROR, "security-context: app_id mismatch");
		failed = 1;
	}

	wl_display_disconnect(sec_display);
	wp_security_context_v1_destroy(ctx);
	wp_security_context_manager_v1_destroy(manager);
	wl_display_roundtrip(conn.display);
	close(close_fds[0]);
	close(close_fds[1]);
	close(listen_fd);
	client_disconnect(&conn);
	return failed;
}
