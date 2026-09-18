// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "treeland-active-notify-unstable-v1-client-protocol.h"

#include <stdio.h>

struct notify_state {
    int activity_count;
    int drag_count;
    uint32_t last_reason;
    uint32_t last_activity_state;
    uint32_t last_drag_state;
};

static void handle_activity_changed(void *data,
                                    struct treeland_active_notify_v1 *notify,
                                    uint32_t reason,
                                    uint32_t state)
{
    (void)notify;
    struct notify_state *ns = data;
    ns->activity_count++;
    ns->last_reason = reason;
    ns->last_activity_state = state;
}

static void handle_drag_changed(void *data,
                                struct treeland_active_notify_v1 *notify,
                                uint32_t state)
{
    (void)notify;
    struct notify_state *ns = data;
    ns->drag_count++;
    ns->last_drag_state = state;
}

static const struct treeland_active_notify_v1_listener notify_listener = {
    .activity_changed = handle_activity_changed,
    .drag_changed = handle_drag_changed,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    if (!client_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = client_bind(&connection, "wl_seat",
                                       &wl_seat_interface, 1);
    struct treeland_active_notify_manager_v1 *manager =
        client_bind(&connection, "treeland_active_notify_manager_v1",
                    &treeland_active_notify_manager_v1_interface, 1);
    if (!seat || !manager) {
        fprintf(stderr, "active-notify: failed to bind globals\n");
        goto failed;
    }

    {
        struct notify_state ns = {0};

        struct treeland_active_notify_v1 *notify =
            treeland_active_notify_manager_v1_get_active_notify(manager, seat);
        if (!notify) {
            fprintf(stderr, "active-notify: get_active_notify(seat) failed\n");
            goto failed;
        }

        treeland_active_notify_v1_add_listener(notify, &notify_listener, &ns);
        if (wl_display_roundtrip(connection.display) == -1) {
            fprintf(stderr, "active-notify: roundtrip failed (no-input check)\n");
            treeland_active_notify_v1_destroy(notify);
            goto failed;
        }

        if (ns.activity_count != 0 || ns.drag_count != 0) {
            fprintf(stderr,
                    "active-notify: unexpected events without real input "
                    "(activity=%d, drag=%d)\n",
                    ns.activity_count, ns.drag_count);
            treeland_active_notify_v1_destroy(notify);
            goto failed;
        }

        // Destroy and re-create the notifier for the same seat.
        treeland_active_notify_v1_destroy(notify);
        if (wl_display_roundtrip(connection.display) == -1) {
            fprintf(stderr, "active-notify: roundtrip failed after destroy\n");
            goto failed;
        }

        struct treeland_active_notify_v1 *notify2 =
            treeland_active_notify_manager_v1_get_active_notify(manager, seat);
        if (!notify2) {
            fprintf(stderr, "active-notify: re-created get_active_notify(seat) failed\n");
            goto failed;
        }

        struct notify_state ns2 = {0};
        treeland_active_notify_v1_add_listener(notify2, &notify_listener, &ns2);
        if (wl_display_roundtrip(connection.display) == -1) {
            fprintf(stderr, "active-notify: roundtrip failed (recreated notifier)\n");
            treeland_active_notify_v1_destroy(notify2);
            goto failed;
        }

        if (ns2.activity_count != 0 || ns2.drag_count != 0) {
            fprintf(stderr,
                    "active-notify: unexpected events on re-created notifier "
                    "(activity=%d, drag=%d)\n",
                    ns2.activity_count, ns2.drag_count);
            treeland_active_notify_v1_destroy(notify2);
            goto failed;
        }

        treeland_active_notify_v1_destroy(notify2);
    }

    treeland_active_notify_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    client_disconnect(&connection);
    return 0;

failed:
    client_disconnect(&connection);
    return 1;
}
