// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "xdg-foreign-unstable-v2-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct foreign_info {
    int has_handle;
    char handle[256];
    int imported_destroyed;
};

static void handle_exported(void *data, struct zxdg_exported_v2 *exported,
                            const char *handle)
{
    (void)exported;
    struct foreign_info *info = data;
    info->has_handle = 1;
    snprintf(info->handle, sizeof(info->handle), "%s", handle);
}

static const struct zxdg_exported_v2_listener exported_listener = {
    .handle = handle_exported,
};

static void handle_imported_destroyed(void *data,
                                      struct zxdg_imported_v2 *imported)
{
    (void)imported;
    struct foreign_info *info = data;
    info->imported_destroyed = 1;
}

static const struct zxdg_imported_v2_listener imported_listener = {
    .destroyed = handle_imported_destroyed,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_toplevel_client tc;
    if (!xdg_toplevel_client_create(&conn, &tc)) {
        fprintf(stderr, "xdg-foreign: failed to create toplevel\n");
        client_disconnect(&conn);
        return 1;
    }

    struct zxdg_exporter_v2 *exporter =
        client_bind(&conn, "zxdg_exporter_v2", &zxdg_exporter_v2_interface, 1);
    struct zxdg_importer_v2 *importer =
        client_bind(&conn, "zxdg_importer_v2", &zxdg_importer_v2_interface, 1);
    if (!exporter || !importer) {
        fprintf(stderr, "xdg-foreign: failed to bind exporter/importer\n");
        goto fail;
    }

    /* Positive: export a mapped toplevel and verify the handle event. */
    struct foreign_info info = {0};
    struct zxdg_exported_v2 *exported =
        zxdg_exporter_v2_export_toplevel(exporter, tc.surface);
    if (!exported) {
        fprintf(stderr, "xdg-foreign: export_toplevel returned null\n");
        goto fail;
    }
    zxdg_exported_v2_add_listener(exported, &exported_listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-foreign: roundtrip after export failed\n");
        goto fail;
    }

    if (!info.has_handle || strlen(info.handle) == 0) {
        fprintf(stderr, "xdg-foreign: did not receive handle event\n");
        goto fail;
    }

    /* Round-trip: import the exported handle and verify the imported object. */
    struct zxdg_imported_v2 *imported =
        zxdg_importer_v2_import_toplevel(importer, info.handle);
    if (!imported) {
        fprintf(stderr, "xdg-foreign: import_toplevel returned null\n");
        goto fail;
    }
    zxdg_imported_v2_add_listener(imported, &imported_listener, &info);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-foreign: roundtrip after import failed\n");
        goto fail;
    }

    /* Destroying the exported surface must notify the imported side. */
    zxdg_exported_v2_destroy(exported);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-foreign: roundtrip after destroy failed\n");
        goto fail;
    }
    if (!info.imported_destroyed) {
        fprintf(stderr, "xdg-foreign: imported did not receive destroyed event\n");
        goto fail;
    }

    zxdg_imported_v2_destroy(imported);
    zxdg_importer_v2_destroy(importer);
    zxdg_exporter_v2_destroy(exporter);
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 0;

fail:
    xdg_toplevel_client_destroy(&tc);
    client_disconnect(&conn);
    return 1;
}
