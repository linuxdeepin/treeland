// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "ext-image-capture-source-v1-client-protocol.h"

#include <stdio.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wl_output *output =
        client_bind(&conn, "wl_output", &wl_output_interface, 1);
    if (!output) {
        fprintf(stderr, "ext-image-capture-source: failed to bind wl_output\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_output_image_capture_source_manager_v1 *out_mgr =
        client_bind(&conn, "ext_output_image_capture_source_manager_v1",
                    &ext_output_image_capture_source_manager_v1_interface, 1);
    if (!out_mgr) {
        fprintf(stderr, "ext-image-capture-source: failed to bind output manager\n");
        client_disconnect(&conn);
        return 1;
    }

    struct ext_image_capture_source_v1 *source =
        ext_output_image_capture_source_manager_v1_create_source(out_mgr, output);
    if (!source) {
        fprintf(stderr, "ext-image-capture-source: create_source returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "ext-image-capture-source: roundtrip failed\n");
        return 1;
    }

    ext_image_capture_source_v1_destroy(source);
    ext_output_image_capture_source_manager_v1_destroy(out_mgr);
    client_disconnect(&conn);
    return 0;
}
