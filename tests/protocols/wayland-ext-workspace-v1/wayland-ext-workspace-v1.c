// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "server-bridge-api.h"
#include "test-state.h"
#include "ext-workspace-v1-client-protocol.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_WORKSPACES 32
struct workspace {
    struct ext_workspace_handle_v1 *proxy;
    char id[128], name[128];
    uint32_t state, capabilities, coordinate;
    int removed;
};
struct client {
    struct client_connection conn;
    struct ext_workspace_manager_v1 *manager;
    struct ext_workspace_group_handle_v1 *group;
    struct wl_output *output;
    int output_entered;
    struct workspace workspaces[MAX_WORKSPACES];
    int count, groups, done, finished, entered, left;
    uint32_t capabilities;
};

static void workspace_id(void *data, struct ext_workspace_handle_v1 *proxy, const char *id)
{ (void)proxy; snprintf(((struct workspace *)data)->id, 128, "%s", id); }
static void workspace_name(void *data, struct ext_workspace_handle_v1 *proxy, const char *name)
{ (void)proxy; snprintf(((struct workspace *)data)->name, 128, "%s", name); }
static void coordinates(void *data, struct ext_workspace_handle_v1 *proxy, struct wl_array *array)
{
    (void)proxy;
    ((struct workspace *)data)->coordinate = array->size == sizeof(uint32_t)
        ? *(uint32_t *)array->data : UINT32_MAX;
}
static void state(void *data, struct ext_workspace_handle_v1 *proxy, uint32_t value)
{ (void)proxy; ((struct workspace *)data)->state = value; }
static void capabilities(void *data, struct ext_workspace_handle_v1 *proxy, uint32_t value)
{ (void)proxy; ((struct workspace *)data)->capabilities = value; }
static void removed(void *data, struct ext_workspace_handle_v1 *proxy)
{ (void)proxy; ((struct workspace *)data)->removed = 1; }
static const struct ext_workspace_handle_v1_listener workspace_listener = {
    .id = workspace_id, .name = workspace_name, .coordinates = coordinates,
    .state = state, .capabilities = capabilities, .removed = removed,
};
static void group_caps(void *data, struct ext_workspace_group_handle_v1 *group, uint32_t caps)
{ (void)group; ((struct client *)data)->capabilities = caps; }
static void output_enter(void *data, struct ext_workspace_group_handle_v1 *group, struct wl_output *output)
{ (void)group; struct client *client = data; if (output == client->output) ++client->output_entered; }
static void output_leave(void *data, struct ext_workspace_group_handle_v1 *group, struct wl_output *output)
{ (void)data; (void)group; (void)output; }
static void workspace_enter(void *data, struct ext_workspace_group_handle_v1 *group, struct ext_workspace_handle_v1 *workspace)
{ (void)group; (void)workspace; ++((struct client *)data)->entered; }
static void workspace_leave(void *data, struct ext_workspace_group_handle_v1 *group, struct ext_workspace_handle_v1 *workspace)
{ (void)group; (void)workspace; ++((struct client *)data)->left; }
static void group_removed(void *data, struct ext_workspace_group_handle_v1 *group)
{ (void)data; (void)group; }
static const struct ext_workspace_group_handle_v1_listener group_listener = {
    .capabilities = group_caps, .output_enter = output_enter, .output_leave = output_leave,
    .workspace_enter = workspace_enter, .workspace_leave = workspace_leave, .removed = group_removed,
};
static void new_group(void *data, struct ext_workspace_manager_v1 *manager, struct ext_workspace_group_handle_v1 *group)
{
    (void)manager;
    struct client *client = data;
    ++client->groups;
    client->group = group;
    ext_workspace_group_handle_v1_add_listener(group, &group_listener, client);
}
static void new_workspace(void *data, struct ext_workspace_manager_v1 *manager, struct ext_workspace_handle_v1 *proxy)
{
    (void)manager;
    struct client *client = data;
    if (client->count == MAX_WORKSPACES) {
        ext_workspace_handle_v1_destroy(proxy);
        return;
    }
    struct workspace *workspace = &client->workspaces[client->count++];
    workspace->proxy = proxy;
    ext_workspace_handle_v1_add_listener(proxy, &workspace_listener, workspace);
}
static void done(void *data, struct ext_workspace_manager_v1 *manager)
{ (void)manager; ++((struct client *)data)->done; }
static void finished(void *data, struct ext_workspace_manager_v1 *manager)
{ (void)manager; ++((struct client *)data)->finished; }
static const struct ext_workspace_manager_v1_listener manager_listener = {
    .workspace_group = new_group, .workspace = new_workspace, .done = done, .finished = finished,
};
static int roundtrip(struct client *client)
{
    return wl_display_roundtrip(client->conn.display) >= 0
        && wl_display_roundtrip(client->conn.display) >= 0;
}
static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
    int32_t width, int32_t height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{
    (void)data; (void)output; (void)x; (void)y; (void)width; (void)height;
    (void)subpixel; (void)make; (void)model; (void)transform;
}
static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{ (void)data; (void)output; (void)flags; (void)width; (void)height; (void)refresh; }
static const struct wl_output_listener output_listener = {
    .geometry = output_geometry, .mode = output_mode,
};
static int connect_client(struct client *client, const char *socket)
{
    if (!client_connect(&client->conn, socket))
        return 0;
    int managers = 0;
    for (uint32_t i = 0; i < client->conn.global_count; ++i)
        managers += strcmp(client->conn.globals[i].interface, "ext_workspace_manager_v1") == 0;
    if (managers != 1)
        return 0;
    client->output = client_bind(&client->conn, "wl_output", &wl_output_interface, 1);
    if (!client->output)
        return 0;
    wl_output_add_listener(client->output, &output_listener, client);
    client->manager = client_bind(&client->conn, "ext_workspace_manager_v1", &ext_workspace_manager_v1_interface, 1);
    if (!client->manager)
        return 0;
    ext_workspace_manager_v1_add_listener(client->manager, &manager_listener, client);
    return roundtrip(client);
}
static void destroy_client(struct client *client)
{
    if (!client->conn.display)
        return;
    for (int i = 0; i < client->count; ++i)
        ext_workspace_handle_v1_destroy(client->workspaces[i].proxy);
    if (client->group)
        ext_workspace_group_handle_v1_destroy(client->group);
    if (client->manager)
        ext_workspace_manager_v1_destroy(client->manager);
    if (client->output)
        wl_output_destroy(client->output);
    client_disconnect(&client->conn);
}
#define REQUIRE(condition, name) do {     if (!(condition)) { TEST_ERROR("FAIL: %s\n", name); failed = 1; goto cleanup; }     printf("PASS: %s\n", name); } while (0)
#define READ() invoke_on_server_thread(workspace_test_read, &production)

int protocol_test_run(const char *socket_name)
{
    struct client a = {0}, b = {0};
    struct workspace_test_state production = {0};
    int failed = 0;
    if (!READ() || !production.background_socket[0])
        return 77;
    REQUIRE(production.independent, "one native manager per session");
    REQUIRE(connect_client(&a, socket_name) && connect_client(&b, production.background_socket), "exactly one visible manager per socket");
    REQUIRE(a.groups == 1 && b.groups == 1 && a.count == 2 && b.count == 2 && a.done && b.done, "initial enumeration and done");
    REQUIRE(a.output_entered == 1 && b.output_entered == 1, "group publishes the bound output");
    REQUIRE(a.entered == 2 && b.entered == 2, "one group contains this session's workspaces");
    REQUIRE(strcmp(a.workspaces[0].name, "session-0-workspace-0") == 0
        && strcmp(b.workspaces[0].name, "session-1-workspace-0") == 0
        && a.workspaces[0].id[0] && strcmp(a.workspaces[0].id, b.workspaces[0].id) != 0, "isolated names and identifiers");
    REQUIRE(a.capabilities == EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE
        && a.workspaces[0].capabilities == EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE
        && a.workspaces[0].coordinate == 0 && a.workspaces[1].coordinate == 1, "capabilities and coordinates");

    ext_workspace_handle_v1_activate(a.workspaces[1].proxy);
    REQUIRE(roundtrip(&a) && READ() && production.current[0] == 0, "activate is deferred until commit");
    ext_workspace_manager_v1_commit(a.manager);
    REQUIRE(roundtrip(&a) && READ() && production.current[0] == 1 && production.current[1] == 0
        && (a.workspaces[1].state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)
        && !(a.workspaces[0].state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE), "commit activates only the owning session");

    ext_workspace_group_handle_v1_create_workspace(a.group, "created-by-client");
    REQUIRE(roundtrip(&a) && READ() && production.counts[0] == 2, "create is deferred until commit");
    ext_workspace_manager_v1_commit(a.manager);
    REQUIRE(roundtrip(&a) && READ() && production.counts[0] == 3 && production.counts[1] == 2
        && a.count == 3 && b.count == 2 && strcmp(a.workspaces[2].name, "created-by-client") == 0
        && (a.workspaces[2].state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE), "create publishes and activates a real model");
    ext_workspace_group_handle_v1_create_workspace(a.group, "created-by-client");
    ext_workspace_manager_v1_commit(a.manager);
    REQUIRE(roundtrip(&a) && READ() && production.counts[0] == 3, "duplicate names are rejected");

    ext_workspace_group_handle_v1_create_workspace(b.group, "background-created-1");
    ext_workspace_manager_v1_commit(b.manager);
    ext_workspace_group_handle_v1_create_workspace(b.group, "background-created-2");
    ext_workspace_manager_v1_commit(b.manager);
    ext_workspace_group_handle_v1_create_workspace(b.group, "background-over-limit");
    ext_workspace_manager_v1_commit(b.manager);
    REQUIRE(roundtrip(&b) && READ() && production.counts[1] == 4 && production.counts[0] == 3
        && production.current[0] == 2 && production.current[1] == 3
        && b.count == 4 && (b.workspaces[3].state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE), "background creation is isolated and respects the workspace limit");

    ext_workspace_handle_v1_activate(a.workspaces[0].proxy);
    REQUIRE(roundtrip(&a) && invoke_on_server_thread(workspace_test_remove_first, NULL) && roundtrip(&a), "remove a model with pending activation");
    ext_workspace_manager_v1_commit(a.manager);
    REQUIRE(roundtrip(&a) && READ() && production.counts[0] == 2 && production.current[0] == 1
        && a.workspaces[0].removed && a.left == 1, "removed workspace request becomes inert");
    char stable_id[128];
    snprintf(stable_id, sizeof(stable_id), "%s", a.workspaces[2].id);
    REQUIRE(invoke_on_server_thread(workspace_test_reorder, NULL) && roundtrip(&a) && READ()
        && production.current[0] == 0 && a.workspaces[2].coordinate == 0
        && (a.workspaces[2].state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)
        && strcmp(stable_id, a.workspaces[2].id) == 0, "reorder updates coordinates without changing identifiers");

    ext_workspace_manager_v1_stop(a.manager);
    REQUIRE(roundtrip(&a) && a.finished == 1 && roundtrip(&b), "stop is acknowledged without affecting another session");
cleanup:
    destroy_client(&a);
    destroy_client(&b);
    return failed;
}
