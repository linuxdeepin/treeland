// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-virtual-output-desktop-v1.h"
#include "treeland-virtual-output-manager-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

extern void virtual_output_desktop_read_state(void *data);

struct virtual_output_client {
    struct client_connection connection;
    struct treeland_virtual_output_manager_v1 *manager;
    struct treeland_virtual_output_v1 *group;
    int outputs_received;
    char group_name[64];
    char outputs[128];
};

static void group_outputs(void *data,
                          struct treeland_virtual_output_v1 *group,
                          const char *name,
                          struct wl_array *outputs)
{
    (void)group;
    struct virtual_output_client *client = data;
    client->outputs_received++;
    snprintf(client->group_name, sizeof(client->group_name), "%s", name ? name : "");
    const char *cursor = outputs->data;
    const char *end = cursor + outputs->size;
    size_t used = 0;
    client->outputs[0] = '\0';
    while (cursor < end && *cursor) {
        const size_t length = strlen(cursor);
        if (used + length + 2 > sizeof(client->outputs))
            break;
        if (used)
            client->outputs[used++] = ' ';
        memcpy(client->outputs + used, cursor, length);
        used += length;
        client->outputs[used] = '\0';
        cursor += length + 1;
    }
}

static void group_error(void *data,
                        struct treeland_virtual_output_v1 *group,
                        uint32_t code,
                        const char *message)
{
    (void)data;
    (void)group;
    (void)code;
    (void)message;
}

static const struct treeland_virtual_output_v1_listener group_listener = {
    .outputs = group_outputs,
    .error = group_error,
};

static int fill_outputs(struct wl_array *array)
{
    static const char *const names[] = { "HEADLESS-1", "HEADLESS-2" };
    wl_array_init(array);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const size_t length = strlen(names[i]) + 1;
        char *slot = wl_array_add(array, length);
        if (!slot) {
            wl_array_release(array);
            return 0;
        }
        memcpy(slot, names[i], length);
    }
    return 1;
}

static void cleanup(struct virtual_output_client *client)
{
    if (client->group)
        treeland_virtual_output_v1_destroy(client->group);
    if (client->manager)
        treeland_virtual_output_manager_v1_destroy(client->manager);
    client_disconnect(&client->connection);
}

int protocol_test_run(const char *socket_name)
{
    struct virtual_output_client client = { 0 };
    struct virtual_output_desktop_state before = { 0 };
    struct virtual_output_desktop_state copied = { 0 };
    struct virtual_output_desktop_state restored = { 0 };
    int result = 1;

    if (!client_connect(&client.connection, socket_name))
        goto done;
    client.manager = client_bind(&client.connection,
                                        "treeland_virtual_output_manager_v1",
                                        &treeland_virtual_output_manager_v1_interface,
                                        2);
    if (!client.manager
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &before)
        || !before.first_present || !before.second_present
        || before.root_output_count != 2 || !before.first_is_normal || !before.second_is_normal)
        goto done;

    struct wl_array outputs;
    if (!fill_outputs(&outputs))
        goto done;
    client.group = treeland_virtual_output_manager_v1_create_virtual_output(
        client.manager, "protocol-copy-group", &outputs);
    wl_array_release(&outputs);
    if (!client.group)
        goto done;
    treeland_virtual_output_v1_add_listener(client.group, &group_listener, &client);

    // The roundtrip orders the create request, Helper::onSetCopyOutput(), and
    // the production outputs event without using a timing delay.
    if (wl_display_roundtrip(client.connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &copied))
        goto done;
    if (client.outputs_received < 1 || strcmp(client.group_name, "protocol-copy-group") != 0
        || strcmp(client.outputs, "HEADLESS-1 HEADLESS-2") != 0
        || !copied.first_present || !copied.second_present || copied.root_output_count != 2
        || !copied.primary_is_first || !copied.first_is_normal || !copied.second_is_copy)
        goto done;

    treeland_virtual_output_v1_destroy(client.group);
    client.group = NULL;
    if (wl_display_roundtrip(client.connection.display) < 0
        || !invoke_on_server_thread(virtual_output_desktop_read_state, &restored))
        goto done;
    if (!restored.first_present || !restored.second_present || restored.root_output_count != 2
        || !restored.primary_is_first || !restored.first_is_normal || !restored.second_is_normal)
        goto done;

    result = 0;
done:
    if (result != 0) {
        fprintf(stderr,
                "virtual output desktop failed: before=(%d,%d root=%d normal=%d,%d) "
                "copied=(%d,%d root=%d primary=%d normal=%d copy=%d event=%d) "
                "restored=(%d,%d root=%d normal=%d,%d)\n",
                before.first_present, before.second_present, before.root_output_count,
                before.first_is_normal, before.second_is_normal,
                copied.first_present, copied.second_present, copied.root_output_count,
                copied.primary_is_first, copied.first_is_normal, copied.second_is_copy,
                client.outputs_received,
                restored.first_present, restored.second_present, restored.root_output_count,
                restored.first_is_normal, restored.second_is_normal);
    }
    cleanup(&client);
    return result;
}
