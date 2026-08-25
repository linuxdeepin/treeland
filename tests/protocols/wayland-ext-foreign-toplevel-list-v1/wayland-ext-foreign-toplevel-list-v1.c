// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the ext_foreign_toplevel_list_v1 global served by Treeland's
// WExtForeignToplevelListV1 wrapper.  After binding, the client calls stop and
// must observe the finished event.  With no mapped toplevels the list is empty,
// so no toplevel events are expected — only finished is asserted.

#include "client-connection.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct list_state {
    int toplevel_count;
    int finished;
};

static void toplevel(void *data, struct ext_foreign_toplevel_list_v1 *list,
                     struct ext_foreign_toplevel_handle_v1 *handle)
{
    (void)list;
    struct list_state *state = data;
    state->toplevel_count++;
    ext_foreign_toplevel_handle_v1_destroy(handle);
}

static void finished(void *data, struct ext_foreign_toplevel_list_v1 *list)
{
    (void)list;
    struct list_state *state = data;
    state->finished = 1;
}

static const struct ext_foreign_toplevel_list_v1_listener list_listener = {
    .toplevel = toplevel,
    .finished = finished,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "ext-foreign-toplevel-list: connect failed\n");
        return 1;
    }

    struct ext_foreign_toplevel_list_v1 *list = client_bind(
        &conn, "ext_foreign_toplevel_list_v1",
        &ext_foreign_toplevel_list_v1_interface, 1);
    if (!list) {
        fprintf(stderr,
                "ext-foreign-toplevel-list: failed to bind global\n");
        client_disconnect(&conn);
        return 1;
    }

    struct list_state state;
    memset(&state, 0, sizeof(state));
    ext_foreign_toplevel_list_v1_add_listener(list, &list_listener, &state);
    ext_foreign_toplevel_list_v1_stop(list);

    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (!state.finished) {
        fprintf(stderr,
                "ext-foreign-toplevel-list: no finished event after stop\n");
        failed = 1;
    }

    client_disconnect(&conn);
    return failed;
}
