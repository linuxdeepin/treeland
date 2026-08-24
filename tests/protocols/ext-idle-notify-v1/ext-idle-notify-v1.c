// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "ext-idle-notify-v1-client-protocol.h"

#include <stdio.h>
#include <unistd.h>

struct idle_info {
    int idled;
    int resumed;
};

static void handle_idled(void *data, struct ext_idle_notification_v1 *n)
{
    (void)n;
    ((struct idle_info *)data)->idled = 1;
}

static void handle_resumed(void *data, struct ext_idle_notification_v1 *n)
{
    (void)n;
    ((struct idle_info *)data)->resumed = 1;
}

static const struct ext_idle_notification_v1_listener listener = {
    .idled = handle_idled,
    .resumed = handle_resumed,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct ext_idle_notifier_v1 *notifier =
        client_bind(&conn, "ext_idle_notifier_v1",
                    &ext_idle_notifier_v1_interface, 1);
    if (!notifier) {
        fprintf(stderr, "ext-idle-notify: failed to bind\n");
        client_disconnect(&conn);
        return 1;
    }

    struct wl_seat *seat =
        client_bind(&conn, "wl_seat", &wl_seat_interface, 1);
    if (!seat) {
        fprintf(stderr, "ext-idle-notify: failed to bind seat\n");
        client_disconnect(&conn);
        return 1;
    }

    /* 1 ms timeout — the notification should fire almost immediately. */
    struct ext_idle_notification_v1 *notif =
        ext_idle_notifier_v1_get_idle_notification(notifier, 1, seat);
    if (!notif) {
        fprintf(stderr, "ext-idle-notify: get_idle_notification returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    struct idle_info info = {0};
    ext_idle_notification_v1_add_listener(notif, &listener, &info);

    /* First roundtrip: server creates the timer (armed for 1 ms). */
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-idle-notify: first roundtrip failed\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Give the server-side timer time to fire (runs in Qt event loop on the
     * main thread while we sleep here). */
    usleep(100000);

    /* Second roundtrip: should deliver the queued idle event. */
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-idle-notify: second roundtrip failed\n");
        client_disconnect(&conn);
        return 1;
    }

    if (!info.idled) {
        fprintf(stderr, "ext-idle-notify: did not receive idle event\n");
        client_disconnect(&conn);
        return 1;
    }

    ext_idle_notification_v1_destroy(notif);
    client_disconnect(&conn);
    return 0;
}
