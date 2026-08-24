// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct handle_info {
    int done;
    int title_received;
    int closed;
};

struct list_info {
    int toplevel_count;
    int done_count;
    struct handle_info handles[16];
};

static void handle_title(void *data, struct ext_foreign_toplevel_handle_v1 *h,
                         const char *title)
{
    (void)h;
    (void)title;
    struct handle_info *info = data;
    info->title_received = 1;
}

static void handle_app_id(void *data, struct ext_foreign_toplevel_handle_v1 *h,
                          const char *app_id)
{
    (void)data; (void)h; (void)app_id;
}

static void handle_identifier(void *data, struct ext_foreign_toplevel_handle_v1 *h,
                              const char *identifier)
{
    (void)data; (void)h; (void)identifier;
}

static void handle_done(void *data, struct ext_foreign_toplevel_handle_v1 *h)
{
    (void)h;
    struct handle_info *info = data;
    info->done = 1;
}

static void handle_closed(void *data, struct ext_foreign_toplevel_handle_v1 *h)
{
    (void)h;
    struct handle_info *info = data;
    info->closed = 1;
}

static const struct ext_foreign_toplevel_handle_v1_listener handle_listener = {
    .title = handle_title,
    .app_id = handle_app_id,
    .identifier = handle_identifier,
    .done = handle_done,
    .closed = handle_closed,
};

static void list_toplevel(void *data, struct ext_foreign_toplevel_list_v1 *list,
                          struct ext_foreign_toplevel_handle_v1 *handle)
{
    (void)list;
    struct list_info *info = data;
    if (info->toplevel_count < 16) {
        struct handle_info *hi = &info->handles[info->toplevel_count];
        ext_foreign_toplevel_handle_v1_add_listener(handle, &handle_listener, hi);
        info->toplevel_count++;
    }
}

static void list_finished(void *data, struct ext_foreign_toplevel_list_v1 *list)
{
    (void)data; (void)list;
}

static const struct ext_foreign_toplevel_list_v1_listener list_listener = {
    .toplevel = list_toplevel,
    .finished = list_finished,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    /* Create a toplevel first so there's something to list. */
    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "ext-foreign-toplevel-list: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_foreign_toplevel_list_v1 *list =
        client_bind(&conn, "ext_foreign_toplevel_list_v1",
                    &ext_foreign_toplevel_list_v1_interface, 1);
    if (!list) {
        fprintf(stderr, "ext-foreign-toplevel-list: failed to bind\n");
        goto fail;
    }

    struct list_info info = {0};
    ext_foreign_toplevel_list_v1_add_listener(list, &list_listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-foreign-toplevel-list: roundtrip failed\n");
        goto fail;
    }

    if (info.toplevel_count == 0) {
        fprintf(stderr, "ext-foreign-toplevel-list: no toplevel events received\n");
        goto fail;
    }

    /* At least one handle should have received done. */
    int any_done = 0;
    for (int i = 0; i < info.toplevel_count; i++) {
        if (info.handles[i].done)
            any_done = 1;
    }
    if (!any_done) {
        fprintf(stderr, "ext-foreign-toplevel-list: no done events received\n");
        goto fail;
    }

    ext_foreign_toplevel_list_v1_destroy(list);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;

fail:
    if (list)
        ext_foreign_toplevel_list_v1_destroy(list);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
