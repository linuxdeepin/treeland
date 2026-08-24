// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "text-input-unstable-v3-client-protocol.h"

#include <stdio.h>

struct ti_info {
    int enter_received;
    int leave_received;
};

static void handle_enter(void *data, struct zwp_text_input_v3 *ti,
                         struct wl_surface *surface)
{
    (void)ti;
    (void)surface;
    struct ti_info *info = data;
    info->enter_received = 1;
}

static void handle_leave(void *data, struct zwp_text_input_v3 *ti,
                         struct wl_surface *surface)
{
    (void)ti;
    (void)surface;
    struct ti_info *info = data;
    info->leave_received = 1;
}

static void handle_preedit_string(void *data, struct zwp_text_input_v3 *ti,
                                  const char *text, int32_t cursor_begin,
                                  int32_t cursor_end)
{
    (void)data;
    (void)ti;
    (void)text;
    (void)cursor_begin;
    (void)cursor_end;
}

static void handle_commit_string(void *data, struct zwp_text_input_v3 *ti,
                                 const char *text)
{
    (void)data;
    (void)ti;
    (void)text;
}

static void handle_delete_surrounding_text(void *data,
                                           struct zwp_text_input_v3 *ti,
                                           uint32_t before_length,
                                           uint32_t after_length)
{
    (void)data;
    (void)ti;
    (void)before_length;
    (void)after_length;
}

static void handle_done(void *data, struct zwp_text_input_v3 *ti,
                        uint32_t serial)
{
    (void)data;
    (void)ti;
    (void)serial;
}

static const struct zwp_text_input_v3_listener listener = {
    .enter = handle_enter,
    .leave = handle_leave,
    .preedit_string = handle_preedit_string,
    .commit_string = handle_commit_string,
    .delete_surrounding_text = handle_delete_surrounding_text,
    .done = handle_done,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "text-input-v3: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    struct zwp_text_input_manager_v3 *manager =
        client_bind(&conn, "zwp_text_input_manager_v3",
                    &zwp_text_input_manager_v3_interface, 1);
    if (!seat || !manager) {
        fprintf(stderr, "text-input-v3: failed to bind required globals\n");
        goto fail;
    }

    /* Positive: create a text-input, enable it, and commit state. */
    struct zwp_text_input_v3 *ti =
        zwp_text_input_manager_v3_get_text_input(manager, seat);
    if (!ti) {
        fprintf(stderr, "text-input-v3: get_text_input returned null\n");
        goto fail;
    }

    struct ti_info info = {0};
    zwp_text_input_v3_add_listener(ti, &listener, &info);

    zwp_text_input_v3_set_surrounding_text(ti, "hello", 5, 5);
    zwp_text_input_v3_set_content_type(ti, 0, 0);
    zwp_text_input_v3_set_cursor_rectangle(ti, 0, 0, 10, 20);
    zwp_text_input_v3_enable(ti);
    zwp_text_input_v3_commit(ti);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "text-input-v3: roundtrip after enable+commit failed\n");
        goto fail;
    }

    /* The headless seat has no keyboard device, so enter/leave are not
     * guaranteed.  The test validates that the enable+commit path does not
     * raise a protocol error. */
    zwp_text_input_v3_disable(ti);
    zwp_text_input_v3_commit(ti);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "text-input-v3: roundtrip after disable+commit failed\n");
        goto fail;
    }

    zwp_text_input_v3_destroy(ti);
    zwp_text_input_manager_v3_destroy(manager);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;

fail:
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
