// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the ext_idle_notifier_v1 global served by Treeland
// (wlr_idle_notifier_v1_create in Helper).  The client requests an idle
// notification with a tiny timeout; since the headless seat has no input
// activity, the server must fire the `idled` event after the timer elapses.

#include "client-connection.h"
#include "ext-idle-notify-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct idle_state {
    int idled;
};

static void idled(void *data, struct ext_idle_notification_v1 *notif)
{
    (void)notif;
    struct idle_state *state = data;
    state->idled = 1;
}

static void resumed(void *data, struct ext_idle_notification_v1 *notif)
{
    (void)data; (void)notif;
}

static const struct ext_idle_notification_v1_listener notif_listener = {
    .idled = idled,
    .resumed = resumed,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "ext-idle-notify: connect failed\n");
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 7);
    if (!seat) {
        fprintf(stderr, "ext-idle-notify: no wl_seat global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_idle_notifier_v1 *notifier = client_bind(
        &conn, "ext_idle_notifier_v1",
        &ext_idle_notifier_v1_interface, 1);
    if (!notifier) {
        fprintf(stderr, "ext-idle-notify: failed to bind notifier\n");
        client_disconnect(&conn);
        return 1;
    }

    struct idle_state state;
    memset(&state, 0, sizeof(state));
    struct ext_idle_notification_v1 *notif =
        ext_idle_notifier_v1_get_idle_notification(notifier, 1 /* ms */, seat);
    if (!notif) {
        fprintf(stderr, "ext-idle-notify: get_idle_notification returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }
    ext_idle_notification_v1_add_listener(notif, &notif_listener, &state);

    /* Pump the server event loop long enough for the 1ms idle timer to fire. */
    for (int i = 0; i < 20 && !state.idled; i++) {
        usleep(50000);
        wl_display_roundtrip(conn.display);
    }

    int failed = 0;
    if (!state.idled) {
        fprintf(stderr, "ext-idle-notify: idled event never fired\n");
        failed = 1;
    }

    ext_idle_notification_v1_destroy(notif);
    ext_idle_notifier_v1_destroy(notifier);
    client_disconnect(&conn);
    return failed;
}
