/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Coverage level E (end-to-end): the client maps an xdg toplevel, creates a
 * zwp_text_input_v3 and enables it, while a second connection acts as a fake
 * zwp_input_method_v2. The test reads back the production WTextInputV3's
 * wlroots handle current_enabled field, proving the text-input enable+commit
 * reached the real compositor text-input pipeline, and asserts the
 * preedit/done/leave events the compositor sends back, covering both the
 * empty-commit clearing path and the clear-before-leave path.
 */

#include "wayland-text-input-unstable-v3.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"
#include "xdg-toplevel-client.h"

#include <string.h>
#include <errno.h>

enum { TEXT_INPUT_V3_MAX_LOG = 64 };

struct text_input_v3_events {
	unsigned int enter;
	unsigned int leave;
	unsigned int preedit_string;
	unsigned int commit_string;
	unsigned int done;
	char preedit[64];
	char committed[64];
	/*
	 * Ordered tags of the events above, so the test can assert that the
	 * clearing preedit and its done arrive before leave: a client that only
	 * receives leave has already dropped its composing state.
	 */
	char log[TEXT_INPUT_V3_MAX_LOG];
	unsigned int log_len;
};

static void note(struct text_input_v3_events *events, char tag)
{
	if (events->log_len + 1 < sizeof(events->log))
		events->log[events->log_len++] = tag;
	events->log[events->log_len] = '\0';
}

static void ti_enter(void *data, struct zwp_text_input_v3 *ti, struct wl_surface *surface)
{
	(void)ti;
	(void)surface;
	struct text_input_v3_events *events = data;
	++events->enter;
	note(events, 'e');
}

static void ti_leave(void *data, struct zwp_text_input_v3 *ti, struct wl_surface *surface)
{
	(void)ti;
	(void)surface;
	struct text_input_v3_events *events = data;
	++events->leave;
	note(events, 'l');
}

static void ti_preedit_string(void *data, struct zwp_text_input_v3 *ti,
                              const char *text, int32_t cursor_begin, int32_t cursor_end)
{
	(void)ti;
	(void)cursor_begin;
	(void)cursor_end;
	struct text_input_v3_events *events = data;
	++events->preedit_string;
	snprintf(events->preedit, sizeof(events->preedit), "%s", text);
	note(events, 'p');
}

static void ti_commit_string(void *data, struct zwp_text_input_v3 *ti, const char *text)
{
	(void)ti;
	struct text_input_v3_events *events = data;
	++events->commit_string;
	snprintf(events->committed, sizeof(events->committed), "%s", text);
	note(events, 'c');
}

static void ti_delete_surrounding_text(void *data, struct zwp_text_input_v3 *ti,
                                       uint32_t before_length, uint32_t after_length)
{
	(void)data;
	(void)ti;
	(void)before_length;
	(void)after_length;
}

static void ti_done(void *data, struct zwp_text_input_v3 *ti, uint32_t serial)
{
	(void)ti;
	(void)serial;
	struct text_input_v3_events *events = data;
	++events->done;
	note(events, 'd');
}

/*
 * wayland-protocols only declares the version 2 events (action, language,
 * preedit_hint) in newer releases. They still have to be filled in when the
 * header declares them: libwayland aborts on a NULL listener function for an
 * event the compositor sends.
 */
#ifdef ZWP_TEXT_INPUT_V3_ACTION_SINCE_VERSION
static void ti_action(void *data, struct zwp_text_input_v3 *ti,
                      uint32_t action, uint32_t serial)
{
	(void)data;
	(void)ti;
	(void)action;
	(void)serial;
}

static void ti_language(void *data, struct zwp_text_input_v3 *ti, const char *language)
{
	(void)data;
	(void)ti;
	(void)language;
}

static void ti_preedit_hint(void *data, struct zwp_text_input_v3 *ti,
                            uint32_t start, uint32_t end, uint32_t hint)
{
	(void)data;
	(void)ti;
	(void)start;
	(void)end;
	(void)hint;
}
#endif

static const struct zwp_text_input_v3_listener ti_listener = {
	.enter = ti_enter,
	.leave = ti_leave,
	.preedit_string = ti_preedit_string,
	.commit_string = ti_commit_string,
	.delete_surrounding_text = ti_delete_surrounding_text,
	.done = ti_done,
#ifdef ZWP_TEXT_INPUT_V3_ACTION_SINCE_VERSION
	.action = ti_action,
	.language = ti_language,
	.preedit_hint = ti_preedit_hint,
#endif
};

struct input_method_state {
	unsigned int activate;
	unsigned int deactivate;
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
	(void)im;
	++((struct input_method_state *)data)->deactivate;
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

static int read_server(struct text_input_v3_server_state *state) {
	return TEST_READ_SERVER(text_input_v3_read_server_state, state,
		"text-input-v3: failed to read server state");
}

int protocol_test_run(const char *socket_name) {
	struct client_connection app = { 0 };
	struct client_connection im_connection = { 0 };
	struct xdg_toplevel_client toplevel = { 0 };
	struct text_input_v3_events events = { 0 };
	struct input_method_state im_state = { 0 };
	struct wl_seat *seat = NULL;
	struct zwp_text_input_manager_v3 *manager = NULL;
	struct zwp_text_input_v3 *ti = NULL;
	struct wl_seat *im_seat = NULL;
	struct zwp_input_method_manager_v2 *im_manager = NULL;
	struct zwp_input_method_v2 *im = NULL;
	struct text_input_v3_server_state srv;
	int stage = 0;

	if (!client_connect(&app, socket_name) || !client_connect(&im_connection, socket_name))
		goto failed;

	/* A mapped toplevel is what makes the compositor send enter. */
	if (!xdg_toplevel_client_create(&app, &toplevel))
		goto failed;
	seat = client_bind(&app, "wl_seat", &wl_seat_interface, 1);
	manager = client_bind(&app, zwp_text_input_manager_v3_interface.name,
	                      &zwp_text_input_manager_v3_interface,
	                      zwp_text_input_manager_v3_interface.version);
	if (seat == NULL || manager == NULL) {
		TEST_ERROR("text-input-v3: failed to bind seat/manager: %s\n", strerror(errno));
		goto failed;
	}

	/* The input method must exist before the text input is enabled, so that
	 * enabling it activates the input method. */
	im_seat = client_bind(&im_connection, "wl_seat", &wl_seat_interface, 1);
	im_manager = client_bind(&im_connection, "zwp_input_method_manager_v2",
	                         &zwp_input_method_manager_v2_interface, 1);
	if (im_seat == NULL || im_manager == NULL)
		goto failed;
	im = zwp_input_method_manager_v2_get_input_method(im_manager, im_seat);
	if (im == NULL)
		goto failed;
	zwp_input_method_v2_add_listener(im, &im_listener, &im_state);
	/* Flush the input method creation before the text input is enabled: the
	 * enable must find an active input method to activate. */
	if (wl_display_roundtrip(im_connection.display) < 0)
		goto failed;

	ti = zwp_text_input_manager_v3_get_text_input(manager, seat);
	if (ti == NULL) {
		TEST_ERROR("text-input-v3: get_text_input returned NULL\n");
		goto failed;
	}
	zwp_text_input_v3_add_listener(ti, &ti_listener, &events);
	zwp_text_input_v3_enable(ti);
	zwp_text_input_v3_commit(ti);
	if (wl_display_roundtrip(app.display) < 0)
		goto failed;
	stage = 1;

	/* E-level: the production text input must have current_enabled == true. */
	if (read_server(&srv))
		goto failed;
	if (!srv.valid) {
		TEST_ERROR("text-input-v3: no WTextInputV3 captured\n");
		goto failed;
	}
	if (!srv.enabled) {
		TEST_ERROR("text-input-v3: current_enabled is false\n");
		goto failed;
	}
	/* Enabling the text input activates the input method. */
	if (wl_display_roundtrip(im_connection.display) < 0)
		goto failed;
	if (im_state.activate != 1 || im_state.unavailable) {
		TEST_ERROR("text-input-v3: activate=%u unavailable=%u\n",
		           im_state.activate, im_state.unavailable);
		goto failed;
	}
	if (events.enter != 1) {
		TEST_ERROR("text-input-v3: enter=%u\n", events.enter);
		goto failed;
	}
	stage = 2;

	/* A commit that sets a preedit reaches the client with its done. */
	zwp_input_method_v2_set_preedit_string(im, "pre", 0, 3);
	zwp_input_method_v2_commit(im, im_state.done);
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(app.display) < 0)
		goto failed;
	if (events.preedit_string != 1 || strcmp(events.preedit, "pre") != 0
	    || events.done != 1 || strcmp(events.log, "epd") != 0) {
		TEST_ERROR("text-input-v3: preedit=%u '%s' done=%u log='%s'\n",
		           events.preedit_string, events.preedit, events.done, events.log);
		goto failed;
	}
	stage = 3;

	/*
	 * A commit that does not call set_preedit_string means "the preedit is
	 * empty": the client must be told to drop its composing text instead of
	 * keeping "pre" forever.
	 */
	zwp_input_method_v2_commit(im, im_state.done);
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(app.display) < 0)
		goto failed;
	if (events.preedit_string != 2 || events.preedit[0] != '\0'
	    || events.done != 2 || strcmp(events.log, "epdpd") != 0) {
		TEST_ERROR("text-input-v3: preedit=%u '%s' done=%u log='%s'\n",
		           events.preedit_string, events.preedit, events.done, events.log);
		goto failed;
	}
	stage = 4;

	/*
	 * Destroying the input method leaves the focused text input, which runs
	 * the same sendLeave() path as a keyboard focus move to another window.
	 * The clearing preedit and its done must arrive before leave.
	 */
	zwp_input_method_v2_destroy(im);
	im = NULL;
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(app.display) < 0)
		goto failed;
	if (events.preedit_string != 3 || events.preedit[0] != '\0'
	    || events.done != 3 || events.leave != 1
	    || strcmp(events.log, "epdpdpdl") != 0) {
		TEST_ERROR("text-input-v3: preedit=%u '%s' done=%u leave=%u log='%s'\n",
		           events.preedit_string, events.preedit, events.done,
		           events.leave, events.log);
		goto failed;
	}
	stage = 5;

	/*
	 * A text input the client already disabled must not get a state update on
	 * focus loss: the compositor may send enter again when the input method
	 * comes back, but no clearing preedit and no done, only leave.
	 */
	const unsigned int log_mark = events.log_len;
	/* A fresh input method restarts its own done counter, which is the serial
	 * its commits have to echo. */
	im_state = (struct input_method_state){ 0 };
	im = zwp_input_method_manager_v2_get_input_method(im_manager, im_seat);
	if (im == NULL)
		goto failed;
	zwp_input_method_v2_add_listener(im, &im_listener, &im_state);
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(app.display) < 0)
		goto failed;
	zwp_text_input_v3_disable(ti);
	zwp_text_input_v3_commit(ti);
	if (wl_display_roundtrip(app.display) < 0)
		goto failed;
	stage = 6;
	zwp_input_method_v2_destroy(im);
	im = NULL;
	if (wl_display_roundtrip(im_connection.display) < 0
	    || wl_display_roundtrip(app.display) < 0)
		goto failed;
	if (events.preedit_string != 3 || events.done != 3 || events.leave != 2
	    || strcmp(events.log + log_mark, "el") != 0) {
		TEST_ERROR("text-input-v3: preedit=%u done=%u leave=%u log='%s'\n",
		           events.preedit_string, events.done, events.leave, events.log);
		goto failed;
	}
	stage = 7;

	zwp_input_method_manager_v2_destroy(im_manager);
	zwp_text_input_v3_destroy(ti);
	zwp_text_input_manager_v3_destroy(manager);
	wl_seat_destroy(im_seat);
	wl_seat_destroy(seat);
	xdg_toplevel_client_destroy(&toplevel);
	client_disconnect(&im_connection);
	client_disconnect(&app);
	return 0;

failed:
	TEST_ERROR("text-input-v3 failure at stage %d: "
	           "preedit=%u '%s' commit=%u '%s' done=%u enter=%u leave=%u log='%s'\n",
	           stage, events.preedit_string, events.preedit, events.commit_string,
	           events.committed, events.done, events.enter, events.leave, events.log);
	if (im)
		zwp_input_method_v2_destroy(im);
	if (im_manager)
		zwp_input_method_manager_v2_destroy(im_manager);
	if (ti)
		zwp_text_input_v3_destroy(ti);
	if (manager)
		zwp_text_input_manager_v3_destroy(manager);
	if (im_seat)
		wl_seat_destroy(im_seat);
	if (seat)
		wl_seat_destroy(seat);
	xdg_toplevel_client_destroy(&toplevel);
	client_disconnect(&im_connection);
	client_disconnect(&app);
	return 1;
}
