// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"

#include <stdio.h>
#include <string.h>

extern void foreign_toplevel_read_server_state(void *data);
extern void foreign_toplevel_render(void *data);

struct foreign_events {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    unsigned int toplevel_count;
    unsigned int title_count;
    unsigned int app_id_count;
    unsigned int state_count;
    unsigned int done_count;
    unsigned int closed_count;
    unsigned int finished_count;
    uint32_t states;
    char title[64];
    char app_id[64];
};

static void handle_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, const char *title)
{
    (void)handle;
    struct foreign_events *events = data;
    snprintf(events->title, sizeof(events->title), "%s", title ? title : "");
    events->title_count++;
}

static void handle_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle, const char *app_id)
{
    (void)handle;
    struct foreign_events *events = data;
    snprintf(events->app_id, sizeof(events->app_id), "%s", app_id ? app_id : "");
    events->app_id_count++;
}

static void handle_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                                struct wl_output *output)
{
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                                struct wl_output *output)
{
    (void)data;
    (void)handle;
    (void)output;
}

static void handle_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                         struct wl_array *state)
{
    (void)handle;
    struct foreign_events *events = data;
    events->states = 0;
    uint32_t *entry;
    wl_array_for_each(entry, state) {
        if (*entry <= ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
            events->states |= 1u << *entry;
    }
    events->state_count++;
}

static void handle_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)handle;
    ((struct foreign_events *)data)->done_count++;
}

static void handle_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)handle;
    ((struct foreign_events *)data)->closed_count++;
}

static void handle_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *handle,
                          struct zwlr_foreign_toplevel_handle_v1 *parent)
{
    (void)data;
    (void)handle;
    (void)parent;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener handle_listener = {
    .title = handle_title,
    .app_id = handle_app_id,
    .output_enter = handle_output_enter,
    .output_leave = handle_output_leave,
    .state = handle_state,
    .done = handle_done,
    .closed = handle_closed,
    .parent = handle_parent,
};

static void manager_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager,
                             struct zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    (void)manager;
    struct foreign_events *events = data;
    events->handle = toplevel;
    events->toplevel_count++;
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &handle_listener, events);
}

static void manager_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *manager)
{
    (void)manager;
    ((struct foreign_events *)data)->finished_count++;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = manager_toplevel,
    .finished = manager_finished,
};

static int read_state(struct foreign_toplevel_server_state *state)
{
    memset(state, 0, sizeof(*state));
    return invoke_on_server_thread(foreign_toplevel_read_server_state, state);
}

static int render_ack_and_read_state(struct client_connection *connection,
                                     struct xdg_toplevel_client *xdg,
                                     struct foreign_toplevel_server_state *state)
{
    return invoke_on_server_thread(foreign_toplevel_render, NULL)
           && xdg_toplevel_client_ack_latest_configure(connection, xdg)
           && invoke_on_server_thread(foreign_toplevel_render, NULL)
           && read_state(state);
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client xdg = { 0 };
    struct foreign_events events = { 0 };
    struct foreign_toplevel_server_state state = { 0 };
    unsigned int stage = 0;
    if (!client_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = client_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct zwlr_foreign_toplevel_manager_v1 *manager = client_bind(
        &connection, "zwlr_foreign_toplevel_manager_v1", &zwlr_foreign_toplevel_manager_v1_interface, 3);
    if (!seat || !manager) {
        fprintf(stderr, "wlr-foreign-toplevel: missing seat or standard manager global\n");
        goto failed;
    }
    stage = 1;
    zwlr_foreign_toplevel_manager_v1_add_listener(manager, &manager_listener, &events);
    if (!xdg_toplevel_client_create(&connection, &xdg))
        goto failed;
    xdg_toplevel_set_title(xdg.toplevel, "foreign protocol test");
    xdg_toplevel_set_app_id(xdg.toplevel, "org.deepin.ForeignProtocolTest");
    stage = 2;
    if (wl_display_roundtrip(connection.display) < 0 || !events.handle)
        goto failed;

    stage = 3;
    if (!read_state(&state) || !state.output_ready || !state.wrapper_created
        || !state.wrapper_in_workspace || events.toplevel_count != 1
        || !events.title_count || !events.app_id_count || !events.state_count || !events.done_count
        || strcmp(events.title, "foreign protocol test") != 0
        || strcmp(events.app_id, "org.deepin.ForeignProtocolTest") != 0) {
        fprintf(stderr, "wlr-foreign-toplevel: initial toplevel events or production wrapper missing\n");
        goto failed;
    }

    zwlr_foreign_toplevel_handle_v1_set_minimized(events.handle);
    stage = 4;
    if (wl_display_roundtrip(connection.display) < 0
        || !render_ack_and_read_state(&connection, &xdg, &state) || !state.minimized)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_unset_minimized(events.handle);
    stage = 5;
    if (wl_display_roundtrip(connection.display) < 0
        || !render_ack_and_read_state(&connection, &xdg, &state) || state.minimized)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_set_maximized(events.handle);
    stage = 6;
    if (wl_display_roundtrip(connection.display) < 0
        || !render_ack_and_read_state(&connection, &xdg, &state) || !state.maximized)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_unset_maximized(events.handle);
    stage = 7;
    if (wl_display_roundtrip(connection.display) < 0
        || !render_ack_and_read_state(&connection, &xdg, &state) || state.maximized)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_set_fullscreen(events.handle, NULL);
    stage = 8;
    if (wl_display_roundtrip(connection.display) < 0
        || !render_ack_and_read_state(&connection, &xdg, &state) || !state.fullscreen)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(events.handle);
    stage = 9;
    if (wl_display_roundtrip(connection.display) < 0
        || !render_ack_and_read_state(&connection, &xdg, &state) || state.fullscreen)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_activate(events.handle, seat);
    stage = 10;
    if (wl_display_roundtrip(connection.display) < 0 || !read_state(&state) || !state.activated || !state.focused)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_set_rectangle(events.handle, xdg.surface, 0, 0, 1, 1);
    zwlr_foreign_toplevel_handle_v1_close(events.handle);
    stage = 11;
    if (wl_display_roundtrip(connection.display) < 0 || !xdg.close_received)
        goto failed;
    zwlr_foreign_toplevel_handle_v1_destroy(events.handle);
    events.handle = NULL;
    zwlr_foreign_toplevel_manager_v1_stop(manager);
    stage = 12;
    if (wl_display_roundtrip(connection.display) < 0 || events.finished_count != 1)
        goto failed;

    xdg_toplevel_client_destroy(&xdg);
    wl_seat_destroy(seat);
    wl_proxy_destroy((struct wl_proxy *)manager);
    client_disconnect(&connection);
    return 0;

failed:
    read_state(&state);
    fprintf(stderr,
            "wlr-foreign-toplevel failure at stage %u: handle=%p events=(toplevel=%u title=%u app-id=%u state=%u done=%u finished=%u) server=(output=%d wrapper=%d workspace=%d minimized=%d maximized=%d fullscreen=%d activated=%d focused=%d maximize-request=%d/%d after-request=%d/%d animation=%d)\n",
            stage, (void *)events.handle, events.toplevel_count, events.title_count,
            events.app_id_count, events.state_count, events.done_count, events.finished_count,
            state.output_ready, state.wrapper_created, state.wrapper_in_workspace, state.minimized,
            state.maximized, state.fullscreen, state.activated, state.focused,
            state.maximize_request_count, state.last_maximize_request,
            state.maximized_after_last_request, state.animation_running_after_last_request,
            state.animation_running);
    if (events.handle)
        zwlr_foreign_toplevel_handle_v1_destroy(events.handle);
    xdg_toplevel_client_destroy(&xdg);
    if (seat)
        wl_seat_destroy(seat);
    if (manager)
        wl_proxy_destroy((struct wl_proxy *)manager);
    client_disconnect(&connection);
    return 1;
}
