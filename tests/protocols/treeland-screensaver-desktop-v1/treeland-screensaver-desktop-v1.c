// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#define _POSIX_C_SOURCE 200809L

#include "treeland-screensaver-desktop-v1.h"
#include "server-bridge-api.h"
#include "ext-idle-notify-v1-client-protocol.h"
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "treeland-screensaver-v1-client-protocol.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern void screensaver_desktop_read_state(void *data);

struct idle_client {
    int idled;
    int resumed;
};

static void idle_notification_idled(void *data, struct ext_idle_notification_v1 *notification)
{
    (void)notification;
    ++((struct idle_client *)data)->idled;
}

static void idle_notification_resumed(void *data, struct ext_idle_notification_v1 *notification)
{
    (void)notification;
    ++((struct idle_client *)data)->resumed;
}

static const struct ext_idle_notification_v1_listener idle_notification_listener = {
    .idled = idle_notification_idled,
    .resumed = idle_notification_resumed,
};

static long elapsed_milliseconds(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) * 1000L + (now.tv_nsec - start->tv_nsec) / 1000000L;
}

static int dispatch_for(struct wl_display *display, int milliseconds)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    const int fd = wl_display_get_fd(display);
    while (elapsed_milliseconds(&start) < milliseconds) {
        if (wl_display_dispatch_pending(display) < 0)
            return 0;
        const int remaining = milliseconds - (int)elapsed_milliseconds(&start);
        struct pollfd pollfd = { .fd = fd, .events = POLLIN };
        const int result = poll(&pollfd, 1, remaining > 0 ? remaining : 0);
        if (result < 0)
            return 0;
        if (result > 0 && wl_display_dispatch(display) < 0)
            return 0;
    }
    return 1;
}

static int read_state(struct screensaver_desktop_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(screensaver_desktop_read_state, state);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct treeland_screensaver_v1 *screensaver = NULL;
    struct ext_idle_notifier_v1 *idle_notifier = NULL;
    struct ext_idle_notification_v1 *notification = NULL;
    struct wl_seat *seat = NULL;
    struct idle_client idle = { 0 };
    struct screensaver_desktop_state state = { 0 };
    int stage = 0;

    if (!client_connect(&connection, socket_name))
        return 1;
    screensaver = client_bind(&connection, "treeland_screensaver_v1",
                                     &treeland_screensaver_v1_interface, 1);
    idle_notifier = client_bind(&connection, "ext_idle_notifier_v1",
                                       &ext_idle_notifier_v1_interface, 1);
    seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    if (!screensaver || !idle_notifier || !seat)
        goto failed;
    stage = 1;
    if (!xdg_toplevel_client_create(&connection, &toplevel))
        goto failed;
    stage = 2;
    if (!read_state(&state) || !state.wrapper_created || !state.wrapper_in_workspace
        || !state.wrapper_visible)
        goto failed;

    treeland_screensaver_v1_inhibit(screensaver, "protocol-test", "desktop idle inhibition");
    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;
    stage = 3;
    notification = ext_idle_notifier_v1_get_idle_notification(idle_notifier, 20, seat);
    if (!notification)
        goto failed;
    ext_idle_notification_v1_add_listener(notification, &idle_notification_listener, &idle);
    if (wl_display_roundtrip(connection.display) < 0 || !dispatch_for(connection.display, 150)
        || idle.idled != 0)
        goto failed;
    stage = 4;

    treeland_screensaver_v1_uninhibit(screensaver);
    if (wl_display_roundtrip(connection.display) < 0 || !dispatch_for(connection.display, 500)
        || idle.idled != 1)
        goto failed;
    stage = 5;

    ext_idle_notification_v1_destroy(notification);
    ext_idle_notifier_v1_destroy(idle_notifier);
    wl_seat_destroy(seat);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 0;

failed:
    fprintf(stderr,
            "screensaver desktop failure at stage %d: wrapper=%d workspace=%d visible=%d idled=%d resumed=%d\n",
            stage, state.wrapper_created, state.wrapper_in_workspace, state.wrapper_visible,
            idle.idled, idle.resumed);
    if (notification) ext_idle_notification_v1_destroy(notification);
    if (idle_notifier) ext_idle_notifier_v1_destroy(idle_notifier);
    if (seat) wl_seat_destroy(seat);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);
    return 1;
}
