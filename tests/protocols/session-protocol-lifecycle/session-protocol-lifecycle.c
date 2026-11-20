// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "session-protocol-lifecycle.h"
#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection survivor = {0};
    if (!client_connect(&survivor, socket_name))
        return 1;

    for (int cycle = 0; cycle < 2; ++cycle) {
        struct lifecycle_state state = {0};
        if (!invoke_on_server_thread(lifecycle_prepare, &state))
            return 1;
        if (state.ready <= 0) {
            fprintf(stderr, "Session protocol lifecycle fixture unavailable\n");
            client_disconnect(&survivor);
            return state.ready == 0 && cycle == 0 ? 77 : 1;
        }
        if (state.observed != 10) {
            fprintf(stderr, "FAIL: expected ten native lifecycle observations, got %d\n", state.observed);
            return 1;
        }
        struct client_connection departing = {0};
        if (!client_connect(&departing, state.socket_name))
            return 1;
        struct client_connection child = {0};
        struct xdg_toplevel_client root_window = {0}, child_window = {0};
        if (!client_connect(&child, state.child_socket_name)
            || !xdg_toplevel_client_create(&departing, &root_window)
            || !xdg_toplevel_client_create(&child, &child_window))
            return 1;
        struct wl_data_device_manager *manager = client_bind(&departing,
            "wl_data_device_manager", &wl_data_device_manager_interface, 3);
        if (!manager)
            return 1;
        struct wl_data_source *source = wl_data_device_manager_create_data_source(manager);
        if (!source || wl_display_roundtrip(departing.display) < 0)
            return 1;

        if (!invoke_on_server_thread(lifecycle_detach, &state) || !state.detached_once) {
            fprintf(stderr, "FAIL: Session::detachInterface did not destroy exactly once\n");
            return 1;
        }
        if (!invoke_on_server_thread(lifecycle_remove, &state)
            || state.destroyed != 8 || state.globals_retired != 2 || !state.interfaces_removed
            || !state.shutdown_with_reference || !state.shutdown_idempotent
            || state.mapped_windows != 2 || !state.windows_released) {
            fprintf(stderr, "FAIL: Session teardown retained native managers or interfaces\n");
            return 1;
        }
        // A commit after logout must fail on both the root and descendant
        // connection while an unrelated Session remains dispatchable.
        wl_surface_commit(root_window.surface);
        wl_surface_commit(child_window.surface);
        if (wl_display_roundtrip(departing.display) >= 0
            || wl_display_roundtrip(child.display) >= 0
            || wl_display_roundtrip(survivor.display) < 0) {
            fprintf(stderr, "FAIL: Session teardown disconnected the wrong client domain\n");
            return 1;
        }
        wl_data_source_destroy(source);
        wl_data_device_manager_destroy(manager);
        xdg_toplevel_client_destroy(&root_window);
        xdg_toplevel_client_destroy(&child_window);
        client_disconnect(&child);
        client_disconnect(&departing);
    }
    client_disconnect(&survivor);
    return 0;
}
