/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the ext_session_lock_manager_v1 global served by Treeland's
 * WSessionLockManager.
 *
 * Coverage level E (end-to-end): the client locks the session and, upon
 * receiving the `locked` event, reads back the production WSessionLock state
 * over the server bridge.  WSessionLock::isLocked() must return true, proving
 * the lock reached the real compositor session-lock object rather than merely
 * surviving without a protocol error.  The compositor may also deny the lock
 * immediately by sending `finished` (e.g. when the lock screen is already
 * visible); in that case the test reports failure, since unconditional lock
 * rejection would be a regression.
 *
 * The same client also covers the lock surface geometry: a top layer surface
 * first reserves an exclusive zone (the panel/dock area), then the lock surface
 * commits a buffer and the production wrapper must still cover the whole output
 * instead of the exclusive-zone-free work area.
 */

#define _POSIX_C_SOURCE 200809L

#include "wayland-ext-session-lock-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "ext-session-lock-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Height of the exclusive zone the test reserves to make the work area differ
 * from the whole output. */
#define PANEL_HEIGHT 40
#define WAIT_TIMEOUT_MS 2000

static struct client_connection conn;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_output *output;
static struct zwlr_layer_shell_v1 *layer_shell;
static struct ext_session_lock_manager_v1 *lock_manager;
static struct wl_surface *panel_surface;
static struct zwlr_layer_surface_v1 *panel;
static struct wl_buffer *panel_buffer;
static struct wl_surface *lock_surface;
static struct ext_session_lock_surface_v1 *lock_surface_v1;
static struct wl_buffer *lock_buffer;
static struct ext_session_lock_v1 *lock;

/* Every observable event the test waits for, and the stage reached for
 * diagnostics. */
static struct {
	int panel_configure;
	uint32_t panel_serial;
	int32_t panel_width;
	int32_t panel_height;
	int locked;
	int finished;
	int lock_surface_configure;
	uint32_t lock_surface_serial;
	int32_t lock_surface_width;
	int32_t lock_surface_height;
} observed;
static unsigned int stage;

static int wait_for_flag(const int *flag, int timeout_ms) {
	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	while (!*flag) {
		if (wl_display_dispatch_pending(conn.display) < 0)
			return 0;
		if (*flag)
			return 1;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		const int elapsed = (int)((now.tv_sec - start.tv_sec) * 1000
			+ (now.tv_nsec - start.tv_nsec) / 1000000);
		const int remaining = timeout_ms - elapsed;
		if (remaining <= 0)
			return 0;

		if (wl_display_flush(conn.display) < 0 && errno != EAGAIN)
			return 0;
		struct pollfd pfd = { .fd = wl_display_get_fd(conn.display), .events = POLLIN };
		if (poll(&pfd, 1, remaining) > 0 && wl_display_dispatch(conn.display) < 0)
			return 0;
	}

	return 1;
}

static void panel_configure(void *data, struct zwlr_layer_surface_v1 *surface,
			    uint32_t serial, uint32_t width, uint32_t height) {
	(void)data;
	(void)surface;
	observed.panel_serial = serial;
	observed.panel_width = (int32_t)width;
	observed.panel_height = (int32_t)height;
	observed.panel_configure++;
}

static void panel_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
	(void)data;
	(void)surface;
}

static const struct zwlr_layer_surface_v1_listener panel_listener = {
	.configure = panel_configure,
	.closed = panel_closed,
};

static void locked(void *data, struct ext_session_lock_v1 *session_lock) {
	(void)data;
	(void)session_lock;
	observed.locked = 1;
}

static void finished(void *data, struct ext_session_lock_v1 *session_lock) {
	(void)data;
	(void)session_lock;
	observed.finished = 1;
}

static const struct ext_session_lock_v1_listener lock_listener = {
	.locked = locked,
	.finished = finished,
};

static void lock_surface_configure(void *data,
				   struct ext_session_lock_surface_v1 *surface,
				   uint32_t serial, uint32_t width, uint32_t height) {
	(void)data;
	(void)surface;
	observed.lock_surface_serial = serial;
	observed.lock_surface_width = (int32_t)width;
	observed.lock_surface_height = (int32_t)height;
	observed.lock_surface_configure++;
}

static const struct ext_session_lock_surface_v1_listener lock_surface_listener = {
	.configure = lock_surface_configure,
};

static struct wl_buffer *make_buffer(int width, int height) {
	/* shm_open takes a literal name, it has no mkstemp-style template expansion:
	 * include the pid like the other protocol tests so concurrent runs (ctest -j)
	 * and leftover objects from a crashed run cannot collide. */
	char name[64];
	snprintf(name, sizeof(name), "/treeland_session_lock_%d", (int)getpid());
	int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return NULL;
	shm_unlink(name);

	const int stride = width * 4;
	const int size = stride * height;
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return NULL;
	}

	void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	memset(p, 0xff, size);

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *buffer = pool
		? wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888)
		: NULL;
	if (pool)
		wl_shm_pool_destroy(pool);
	munmap(p, size);
	close(fd);
	return buffer;
}

static int read_server_state(struct session_lock_server_state *state) {
	if (!invoke_on_server_thread(session_lock_read_server_state, state)) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to read server state\n",
			stage);
		return 0;
	}
	return 1;
}

static int run(void) {
	struct session_lock_server_state srv = { 0 };

	/* 1. Reserve a panel area so validGeometry() differs from geometry(): without
	 *    an exclusive zone the surface-size assertion below would pass trivially. */
	stage = 1;
	panel_surface = wl_compositor_create_surface(compositor);
	if (!panel_surface) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to create panel surface\n",
			stage);
		return 1;
	}

	panel = zwlr_layer_shell_v1_get_layer_surface(layer_shell,
						      panel_surface,
						      output,
						      ZWLR_LAYER_SHELL_V1_LAYER_TOP,
						      "protocol-test-panel");
	if (!panel) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to create panel layer surface\n",
			stage);
		return 1;
	}
	zwlr_layer_surface_v1_add_listener(panel, &panel_listener, NULL);
	zwlr_layer_surface_v1_set_size(panel, 0, PANEL_HEIGHT);
	zwlr_layer_surface_v1_set_anchor(panel,
					 ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
						 | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
						 | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_exclusive_zone(panel, PANEL_HEIGHT);
	wl_surface_commit(panel_surface);

	if (!wait_for_flag(&observed.panel_configure, WAIT_TIMEOUT_MS)) {
		fprintf(stderr, "ext-session-lock failure at stage %u: panel never configured\n",
			stage);
		return 1;
	}
	zwlr_layer_surface_v1_ack_configure(panel, observed.panel_serial);
	panel_buffer = make_buffer(observed.panel_width, observed.panel_height);
	if (!panel_buffer) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to create panel buffer\n",
			stage);
		return 1;
	}
	wl_surface_attach(panel_surface, panel_buffer, 0, 0);
	wl_surface_damage(panel_surface, 0, 0, observed.panel_width, observed.panel_height);
	wl_surface_commit(panel_surface);
	if (wl_display_roundtrip(conn.display) < 0) {
		fprintf(stderr, "ext-session-lock failure at stage %u: panel commit failed\n", stage);
		return 1;
	}

	/* 2. Precondition: the panel exclusive zone must actually shrink the work area. */
	stage = 2;
	if (!read_server_state(&srv))
		return 1;
	if (srv.valid_area_height != srv.output_height - PANEL_HEIGHT) {
		fprintf(stderr,
			"ext-session-lock failure at stage %u: exclusive zone not applied "
			"(output height %d, work area height %d, expected %d)\n",
			stage, srv.output_height, srv.valid_area_height,
			srv.output_height - PANEL_HEIGHT);
		return 1;
	}

	/* 3. Lock the session. */
	stage = 3;
	lock = ext_session_lock_manager_v1_lock(lock_manager);
	if (!lock) {
		fprintf(stderr, "ext-session-lock failure at stage %u: lock() returned NULL\n", stage);
		return 1;
	}
	ext_session_lock_v1_add_listener(lock, &lock_listener, NULL);

	if (!wait_for_flag(&observed.locked, WAIT_TIMEOUT_MS)) {
		if (observed.finished)
			fprintf(stderr,
				"ext-session-lock failure at stage %u: compositor rejected the lock\n",
				stage);
		else
			fprintf(stderr,
				"ext-session-lock failure at stage %u: no locked or finished event\n",
				stage);
		return 1;
	}

	/* 4. Commit a lock surface buffer at the configured size. */
	stage = 4;
	lock_surface = wl_compositor_create_surface(compositor);
	if (!lock_surface) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to create lock surface\n",
			stage);
		return 1;
	}
	lock_surface_v1 = ext_session_lock_v1_get_lock_surface(lock, lock_surface, output);
	if (!lock_surface_v1) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to get lock surface\n",
			stage);
		return 1;
	}
	ext_session_lock_surface_v1_add_listener(lock_surface_v1, &lock_surface_listener, NULL);

	if (!wait_for_flag(&observed.lock_surface_configure, WAIT_TIMEOUT_MS)) {
		fprintf(stderr, "ext-session-lock failure at stage %u: lock surface never configured\n",
			stage);
		return 1;
	}
	ext_session_lock_surface_v1_ack_configure(lock_surface_v1, observed.lock_surface_serial);
	lock_buffer = make_buffer(observed.lock_surface_width, observed.lock_surface_height);
	if (!lock_buffer) {
		fprintf(stderr, "ext-session-lock failure at stage %u: failed to create lock buffer\n",
			stage);
		return 1;
	}
	wl_surface_attach(lock_surface, lock_buffer, 0, 0);
	wl_surface_damage(lock_surface, 0, 0,
			  observed.lock_surface_width, observed.lock_surface_height);
	wl_surface_commit(lock_surface);
	if (wl_display_roundtrip(conn.display) < 0) {
		fprintf(stderr, "ext-session-lock failure at stage %u: lock surface commit failed\n",
			stage);
		return 1;
	}

	/* 5. The production wrapper must cover the whole output, not the work area. */
	stage = 5;
	if (!read_server_state(&srv))
		return 1;
	if (!srv.valid || !srv.locked) {
		fprintf(stderr,
			"ext-session-lock failure at stage %u: WSessionLock state invalid=%d locked=%d\n",
			stage, srv.valid, srv.locked);
		return 1;
	}
	if (!srv.wrapper_valid) {
		fprintf(stderr, "ext-session-lock failure at stage %u: no lock surface wrapper\n",
			stage);
		return 1;
	}
	if (observed.lock_surface_width != srv.output_width
	    || observed.lock_surface_height != srv.output_height) {
		fprintf(stderr,
			"ext-session-lock failure at stage %u: configured %dx%d, output is %dx%d\n",
			stage, observed.lock_surface_width, observed.lock_surface_height,
			srv.output_width, srv.output_height);
		return 1;
	}
	if (srv.wrapper_width != srv.output_width || srv.wrapper_height != srv.output_height) {
		fprintf(stderr,
			"ext-session-lock failure at stage %u: lock surface wrapper is %dx%d at (%d,%d), "
			"expected the whole %dx%d output (work area height %d)\n",
			stage, srv.wrapper_width, srv.wrapper_height, srv.wrapper_x, srv.wrapper_y,
			srv.output_width, srv.output_height, srv.valid_area_height);
		return 1;
	}
	if (srv.wrapper_x != srv.output_x || srv.wrapper_y != srv.output_y) {
		fprintf(stderr,
			"ext-session-lock failure at stage %u: lock surface wrapper at (%d,%d), "
			"expected the output origin (%d,%d)\n",
			stage, srv.wrapper_x, srv.wrapper_y, srv.output_x, srv.output_y);
		return 1;
	}

	return 0;
}

static void cleanup(void) {
	/* Destroy the lock surface before the lock: unlocking destroys the lock's
	 * surfaces server side, so destroying it afterwards would send a request on
	 * an already destroyed object and raise a protocol error. */
	if (lock_surface_v1)
		ext_session_lock_surface_v1_destroy(lock_surface_v1);
	if (lock)
		ext_session_lock_v1_unlock_and_destroy(lock);
	if (lock_surface)
		wl_surface_destroy(lock_surface);
	if (lock_buffer)
		wl_buffer_destroy(lock_buffer);
	if (panel)
		zwlr_layer_surface_v1_destroy(panel);
	if (panel_surface)
		wl_surface_destroy(panel_surface);
	if (panel_buffer)
		wl_buffer_destroy(panel_buffer);
	if (layer_shell)
		zwlr_layer_shell_v1_destroy(layer_shell);
	if (lock_manager)
		ext_session_lock_manager_v1_destroy(lock_manager);
	if (output)
		wl_output_destroy(output);
	if (shm)
		wl_shm_destroy(shm);
	if (compositor)
		wl_compositor_destroy(compositor);
	if (conn.display)
		wl_display_flush(conn.display);
}

int protocol_test_run(const char *socket_name) {
	memset(&conn, 0, sizeof(conn));
	if (!client_connect(&conn, socket_name)) {
		TEST_ERROR("ext-session-lock: connect failed\n");
		return 1;
	}

	compositor = client_bind(&conn, wl_compositor_interface.name, &wl_compositor_interface,
				 wl_compositor_interface.version);
	shm = client_bind(&conn, wl_shm_interface.name, &wl_shm_interface,
			  wl_shm_interface.version);
	output = client_bind(&conn, wl_output_interface.name, &wl_output_interface,
			     wl_output_interface.version);
	layer_shell = client_bind(&conn, zwlr_layer_shell_v1_interface.name,
				  &zwlr_layer_shell_v1_interface, 5);
	lock_manager = client_bind(&conn, ext_session_lock_manager_v1_interface.name,
				   &ext_session_lock_manager_v1_interface,
				   ext_session_lock_manager_v1_interface.version);
	if (!compositor || !shm || !output || !layer_shell || !lock_manager) {
		TEST_ERROR("ext-session-lock: failed to bind required globals: %s\n",
			   strerror(errno));
		cleanup();
		client_disconnect(&conn);
		return 1;
	}

	const int failed = run();
	cleanup();
	client_disconnect(&conn);
	return failed;
}
