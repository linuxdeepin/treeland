/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client creates a zwp_text_input_v1,
 * maps a toplevel to get keyboard focus, and calls activate(surface, seat).
 * The test reads back the production WTextInputV1's activate() signal state,
 * verifying it was emitted, proving the text-input activation request reached
 * the real compositor text-input pipeline. A second connection acts as a fake
 * zwp_input_method_v2, so the compositor forwards input-method commits to the
 * v1 text input: the client asserts that a commit which does not set a preedit
 * produces an empty preedit_string carrying the serial from commit_state, and
 * that leave does not clear the preedit (v1 has no done event, and its
 * preedit_string serial is only meaningful for a commit).
 */

#include "wayland-text-input-unstable-v1.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "text-input-unstable-v1-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"

#include <string.h>
#include <errno.h>

enum { TEXT_INPUT_V1_MAX_LOG = 64 };

struct text_input_v1_events {
	unsigned int enter;
	unsigned int leave;
	unsigned int preedit_string;
	unsigned int preedit_styling;
	unsigned int preedit_cursor;
	unsigned int commit_string;
	uint32_t preedit_serial;
	char preedit[64];
	char committed[64];
	/* Ordered tags of the preedit/commit/leave events. */
	char log[TEXT_INPUT_V1_MAX_LOG];
	unsigned int log_len;
};

static void note(struct text_input_v1_events *events, char tag)
{
	if (events->log_len + 1 < sizeof(events->log))
		events->log[events->log_len++] = tag;
	events->log[events->log_len] = '\0';
}

static void ti_enter(void *data, struct zwp_text_input_v1 *ti, struct wl_surface *surface)
{
	(void)ti;
	(void)surface;
	struct text_input_v1_events *events = data;
	++events->enter;
	note(events, 'e');
}

static void ti_leave(void *data, struct zwp_text_input_v1 *ti)
{
	(void)ti;
	struct text_input_v1_events *events = data;
	++events->leave;
	note(events, 'l');
}

static void ti_modifiers_map(void *data, struct zwp_text_input_v1 *ti, struct wl_array *map)
{
	(void)data;
	(void)ti;
	(void)map;
}

static void ti_input_panel_state(void *data, struct zwp_text_input_v1 *ti, uint32_t state)
{
	(void)data;
	(void)ti;
	(void)state;
}

static void ti_preedit_string(void *data, struct zwp_text_input_v1 *ti,
                              uint32_t serial, const char *text, const char *commit)
{
	(void)ti;
	(void)commit;
	struct text_input_v1_events *events = data;
	++events->preedit_string;
	events->preedit_serial = serial;
	snprintf(events->preedit, sizeof(events->preedit), "%s", text);
	note(events, 'p');
}

static void ti_preedit_styling(void *data, struct zwp_text_input_v1 *ti,
                               uint32_t index, uint32_t length, uint32_t style)
{
	(void)ti;
	(void)index;
	(void)length;
	(void)style;
	++((struct text_input_v1_events *)data)->preedit_styling;
}

static void ti_preedit_cursor(void *data, struct zwp_text_input_v1 *ti, int32_t index)
{
	(void)ti;
	(void)index;
	++((struct text_input_v1_events *)data)->preedit_cursor;
}

static void ti_commit_string(void *data, struct zwp_text_input_v1 *ti,
                             uint32_t serial, const char *text)
{
	(void)ti;
	(void)serial;
	struct text_input_v1_events *events = data;
	++events->commit_string;
	snprintf(events->committed, sizeof(events->committed), "%s", text);
	note(events, 'k');
}

static void ti_cursor_position(void *data, struct zwp_text_input_v1 *ti,
                               int32_t index, int32_t anchor)
{
	(void)data;
	(void)ti;
	(void)index;
	(void)anchor;
}

static void ti_delete_surrounding_text(void *data, struct zwp_text_input_v1 *ti,
                                       int32_t index, uint32_t length)
{
	(void)data;
	(void)ti;
	(void)index;
	(void)length;
}

static void ti_keysym(void *data, struct zwp_text_input_v1 *ti, uint32_t serial,
                      uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
{
	(void)data;
	(void)ti;
	(void)serial;
	(void)time;
	(void)sym;
	(void)state;
	(void)modifiers;
}

static void ti_language(void *data, struct zwp_text_input_v1 *ti,
                        uint32_t serial, const char *language)
{
	(void)data;
	(void)ti;
	(void)serial;
	(void)language;
}

static void ti_text_direction(void *data, struct zwp_text_input_v1 *ti,
                              uint32_t serial, uint32_t direction)
{
	(void)data;
	(void)ti;
	(void)serial;
	(void)direction;
}

static const struct zwp_text_input_v1_listener ti_listener = {
	.enter = ti_enter,
	.leave = ti_leave,
	.modifiers_map = ti_modifiers_map,
	.input_panel_state = ti_input_panel_state,
	.preedit_string = ti_preedit_string,
	.preedit_styling = ti_preedit_styling,
	.preedit_cursor = ti_preedit_cursor,
	.commit_string = ti_commit_string,
	.cursor_position = ti_cursor_position,
	.delete_surrounding_text = ti_delete_surrounding_text,
	.keysym = ti_keysym,
	.language = ti_language,
	.text_direction = ti_text_direction,
};

struct input_method_state {
	unsigned int activate;
	unsigned int done;
	unsigned int unavailable;
};

static void im_activate(void *data, struct zwp_input_method_v2 *im)
{
	(void)im;
	++((struct input_method_state *)data)->activate;
}

static void im_deactivate(void *data, struct zwp_input_method_v2 *im)
{
	(void)data;
	(void)im;
}

static void im_surrounding_text(void *data, struct zwp_input_method_v2 *im,
                                const char *text, uint32_t cursor, uint32_t anchor)
{
	(void)data;
	(void)im;
	(void)text;
	(void)cursor;
	(void)anchor;
}

static void im_text_change_cause(void *data, struct zwp_input_method_v2 *im, uint32_t cause)
{
	(void)data;
	(void)im;
	(void)cause;
}

static void im_content_type(void *data, struct zwp_input_method_v2 *im,
                            uint32_t hint, uint32_t purpose)
{
	(void)data;
	(void)im;
	(void)hint;
	(void)purpose;
}

static void im_done(void *data, struct zwp_input_method_v2 *im)
{
	(void)im;
	/* The input method echoes the number of done events back as the commit
	 * serial. */
	++((struct input_method_state *)data)->done;
}

static void im_unavailable(void *data, struct zwp_input_method_v2 *im)
{
	(void)im;
	++((struct input_method_state *)data)->unavailable;
}

static const struct zwp_input_method_v2_listener im_listener = {
	.activate = im_activate,
	.deactivate = im_deactivate,
	.surrounding_text = im_surrounding_text,
	.text_change_cause = im_text_change_cause,
	.content_type = im_content_type,
	.done = im_done,
	.unavailable = im_unavailable,
};

static int read_server(struct text_input_v1_server_state *state) {
	return TEST_READ_SERVER(text_input_v1_read_server_state, state,
		"text-input-v1: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection conn = { 0 };
	struct client_connection im_connection = { 0 };
	struct client_seat client_seat = { 0 };
	struct xdg_toplevel_client tc = { 0 };
	struct text_input_v1_events events = { 0 };
	struct input_method_state im_state = { 0 };
	struct wl_seat *seat = NULL;
	struct zwp_text_input_manager_v1 *manager = NULL;
	struct zwp_text_input_v1 *ti = NULL;
	struct wl_seat *im_seat = NULL;
	struct zwp_input_method_manager_v2 *im_manager = NULL;
	struct zwp_input_method_v2 *im = NULL;
	struct text_input_v1_server_state srv;
	/* Serial the client passes to commit_state; the compositor must echo it
	 * back in preedit_string/commit_string. */
	const uint32_t commit_state_serial = 7;
	int stage = 0;

	if (!client_connect(&conn, socket_name) || !client_connect(&im_connection, socket_name))
		goto failed;

	if (!client_bind_seat(&conn, wl_seat_interface.version, &client_seat)) {
		TEST_ERROR("text-input-v1: no wl_seat global\n");
		goto failed;
	}
	seat = client_seat.seat;

	/* Map a toplevel so we have a surface to activate text input on. */
	if (!xdg_toplevel_client_create_pending(&conn, &tc)) {
		TEST_ERROR("text-input-v1: create_pending failed\n");
		goto failed;
	}
	if (!xdg_toplevel_client_complete_map(&conn, &tc)) {
		TEST_ERROR("text-input-v1: complete_map failed\n");
		goto failed;
	}
	if (wl_display_roundtrip(conn.display) < 0)
		goto failed;

	manager = client_bind(&conn, zwp_text_input_manager_v1_interface.name,
	                      &zwp_text_input_manager_v1_interface,
	                      zwp_text_input_manager_v1_interface.version);
	if (manager == NULL) {
		TEST_ERROR("text-input-v1: failed to bind manager: %s\n", strerror(errno));
		goto failed;
	}

	ti = zwp_text_input_manager_v1_create_text_input(manager);
	if (ti == NULL) {
		TEST_ERROR("text-input-v1: create_text_input returned NULL\n");
		goto failed;
	}
	zwp_text_input_v1_add_listener(ti, &ti_listener, &events);

	/* The input method must exist before the text input is activated, so that
	 * activating it activates the input method. */
	im_seat = client_bind(&im_connection, "wl_seat", &wl_seat_interface, 1);
	im_manager = client_bind(&im_connection, "zwp_input_method_manager_v2",
	                         &zwp_input_method_manager_v2_interface, 1);
	if (im_seat == NULL || im_manager == NULL)
		goto failed;
	im = zwp_input_method_manager_v2_get_input_method(im_manager, im_seat);
	if (im == NULL)
		goto failed;
	zwp_input_method_v2_add_listener(im, &im_listener, &im_state);
	if (wl_display_roundtrip(im_connection.display) < 0)
		goto failed;

	zwp_text_input_v1_activate(ti, seat, tc.surface);
	if (wl_display_roundtrip(conn.display) < 0)
		goto failed;
	stage = 1;

	/* E-level: the production text input must have been activated. */
	if (read_server(&srv))
		goto failed;
	if (!srv.valid) {
		TEST_ERROR("text-input-v1: manager not found\n");
		goto failed;
	}
	if (!srv.activated) {
		TEST_ERROR("text-input-v1: activate signal not emitted\n");
		goto failed;
	}
	if (events.enter != 1) {
		TEST_ERROR("text-input-v1: enter=%u\n", events.enter);
		goto failed;
	}
	/* Activating the text input activates the input method. */
	if (wl_display_roundtrip(im_connection.display) < 0)
		goto failed;
	if (im_state.activate != 1 || im_state.unavailable) {
		TEST_ERROR("text-input-v1: activate=%u unavailable=%u\n",
		           im_state.activate, im_state.unavailable);
		goto failed;
	}

	/* commit_state carries the serial the compositor must echo back. It also
	 * makes the compositor send another done to the input method, so the
	 * commit serial below has to be read after this roundtrip. */
	zwp_text_input_v1_commit_state(ti, commit_state_serial);
	if (wl_display_roundtrip(conn.display) < 0
	    || wl_display_roundtrip(im_connection.display) < 0)
		goto failed;
	stage = 2;

	/* A commit that sets a preedit reaches the v1 client. */
	zwp_input_method_v2_set_preedit_string(im, "pre", 0, 3);
	zwp_input_method_v2_commit(im, im_state.done);
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(conn.display) < 0)
		goto failed;
	if (events.preedit_string != 1 || strcmp(events.preedit, "pre") != 0
	    || events.preedit_serial != commit_state_serial
	    || events.preedit_styling != 1 || events.preedit_cursor != 1
	    || strcmp(events.log, "ekp") != 0) {
		TEST_ERROR("text-input-v1: preedit=%u '%s' serial=%u styling=%u cursor=%u log='%s'\n",
		           events.preedit_string, events.preedit, events.preedit_serial,
		           events.preedit_styling, events.preedit_cursor, events.log);
		goto failed;
	}
	stage = 3;

	/*
	 * A commit that does not call set_preedit_string means "the preedit is
	 * empty": v1 has no done event, so an empty preedit_string is the only way
	 * to tell the client to drop its composing text.
	 */
	zwp_input_method_v2_commit(im, im_state.done);
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(conn.display) < 0)
		goto failed;
	if (events.preedit_string != 2 || events.preedit[0] != '\0'
	    || events.preedit_serial != commit_state_serial
	    || strcmp(events.log, "ekpkp") != 0) {
		TEST_ERROR("text-input-v1: preedit=%u '%s' serial=%u log='%s'\n",
		           events.preedit_string, events.preedit,
		           events.preedit_serial, events.log);
		goto failed;
	}
	stage = 4;

	/*
	 * v1 leave deliberately does not clear the preedit: its preedit_string
	 * serial comes from commit_state and has no defined meaning for a focus
	 * loss, so only leave is sent.
	 */
	zwp_input_method_v2_destroy(im);
	im = NULL;
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(conn.display) < 0)
		goto failed;
	if (events.preedit_string != 2 || events.leave != 1
	    || strcmp(events.log, "ekpkpl") != 0) {
		TEST_ERROR("text-input-v1: preedit=%u leave=%u log='%s'\n",
		           events.preedit_string, events.leave, events.log);
		goto failed;
	}
	stage = 5;

	zwp_text_input_v1_deactivate(ti, seat);
	zwp_text_input_v1_destroy(ti);
	zwp_text_input_manager_v1_destroy(manager);
	zwp_input_method_manager_v2_destroy(im_manager);
	wl_seat_destroy(im_seat);
	xdg_toplevel_client_destroy(&tc);
	wl_seat_destroy(seat);
	client_disconnect(&im_connection);
	client_disconnect(&conn);
	return 0;

failed:
	TEST_ERROR("text-input-v1 failure at stage %d: "
	           "preedit=%u '%s' serial=%u commit=%u '%s' enter=%u leave=%u log='%s'\n",
	           stage, events.preedit_string, events.preedit, events.preedit_serial,
	           events.commit_string, events.committed, events.enter, events.leave,
	           events.log);
	if (im)
		zwp_input_method_v2_destroy(im);
	if (im_manager)
		zwp_input_method_manager_v2_destroy(im_manager);
	if (ti)
		zwp_text_input_v1_destroy(ti);
	if (manager)
		zwp_text_input_manager_v1_destroy(manager);
	if (im_seat)
		wl_seat_destroy(im_seat);
	if (tc.surface)
		xdg_toplevel_client_destroy(&tc);
	if (seat)
		wl_seat_destroy(seat);
	client_disconnect(&im_connection);
	client_disconnect(&conn);
	return 1;
}
