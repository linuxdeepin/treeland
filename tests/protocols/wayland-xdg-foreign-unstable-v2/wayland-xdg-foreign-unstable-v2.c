// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Test the zxdg_exporter_v2 / zxdg_importer_v2 globals served by Treeland
// (wlr_xdg_foreign_v2_create).  The client exports a surface, asserts the
// `handle` event delivers a non-empty handle, then imports that handle and
// asserts an imported resource is created — exercising the real export/import
// round-trip.

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-foreign-unstable-v2-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct export_state {
    int handle_received;
    char handle[256];
};

static void exported_handle(void *data, struct zxdg_exported_v2 *exported,
                            const char *handle)
{
    (void)exported;
    struct export_state *state = data;
    state->handle_received = 1;
    if (handle) {
        strncpy(state->handle, handle, sizeof(state->handle) - 1);
        state->handle[sizeof(state->handle) - 1] = '\0';
    }
}

static const struct zxdg_exported_v2_listener exported_listener = {
    .handle = exported_handle,
};

static void imported_destroyed(void *data, struct zxdg_imported_v2 *imported)
{
    (void)data; (void)imported;
}

static const struct zxdg_imported_v2_listener imported_listener = {
    .destroyed = imported_destroyed,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name)) {
        fprintf(stderr, "xdg-foreign-v2: connect failed\n");
        return 1;
    }

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "xdg-foreign-v2: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct export_state state;
    memset(&state, 0, sizeof(state));
    struct zxdg_exporter_v2 *exporter = client_bind(
        &conn, "zxdg_exporter_v2", &zxdg_exporter_v2_interface, 1);
    struct zxdg_importer_v2 *importer = client_bind(
        &conn, "zxdg_importer_v2", &zxdg_importer_v2_interface, 1);
    if (!exporter || !importer) {
        fprintf(stderr, "xdg-foreign-v2: failed to bind exporter/importer\n");
        xdg_toplevel_client_destroy(&tc);
        client_disconnect(&conn);
        return 1;
    }

    struct zxdg_exported_v2 *exported =
        zxdg_exporter_v2_export_toplevel(exporter, tc.surface);
    if (!exported) {
        fprintf(stderr, "xdg-foreign-v2: export_toplevel returned NULL\n");
        client_disconnect(&conn);
        return 1;
    }
    zxdg_exported_v2_add_listener(exported, &exported_listener, &state);
    wl_display_roundtrip(conn.display);

    int failed = 0;
    if (!state.handle_received || state.handle[0] == '\0') {
        fprintf(stderr, "xdg-foreign-v2: no handle event / empty handle\n");
        failed = 1;
    } else {
        struct zxdg_imported_v2 *imported =
            zxdg_importer_v2_import_toplevel(importer, state.handle);
        if (!imported) {
            fprintf(stderr, "xdg-foreign-v2: import_toplevel returned NULL\n");
            failed = 1;
        } else {
            zxdg_imported_v2_add_listener(imported, &imported_listener, NULL);
            wl_display_roundtrip(conn.display);
            zxdg_imported_v2_destroy(imported);
        }
    }

    zxdg_exported_v2_destroy(exported);
    xdg_toplevel_client_destroy(&tc);
    zxdg_importer_v2_destroy(importer);
    zxdg_exporter_v2_destroy(exporter);
    client_disconnect(&conn);
    return failed;
}
