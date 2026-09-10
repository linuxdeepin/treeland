/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * Test the ext_image_copy_capture_manager_v1 + ext_output_image_capture_source_manager_v1
 * globals served by Treeland (wlr_ext_image_copy_capture_manager_v1_create and
 * wlr_ext_output_image_capture_source_manager_v1_create in Helper).
 * The client binds both managers and verifies they are advertised by the
 * compositor.  (Creating a capture session in headless mode can deadlock in
 * wlr_output_configure_primary_swapchain, so we test global presence only.)
 */

#include "client-connection.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "ext-image-capture-source-v1-client-protocol.h"

#include <wlr/util/log.h>

int protocol_test_run(const char *socket_name) {
	struct client_connection conn;
	if (!client_connect(&conn, socket_name)) {
		wlr_log(WLR_ERROR, "ext-image-copy-capture: connect failed");
		return 1;
	}

	struct ext_output_image_capture_source_manager_v1 *src_mgr =
		client_bind(&conn, ext_output_image_capture_source_manager_v1_interface.name, &ext_output_image_capture_source_manager_v1_interface, 1);
	struct ext_image_copy_capture_manager_v1 *cap_mgr = client_bind(&conn,
		ext_image_copy_capture_manager_v1_interface.name, &ext_image_copy_capture_manager_v1_interface, 1);
	if (src_mgr == NULL || cap_mgr == NULL) {
		wlr_log_errno(WLR_ERROR, "ext-image-copy-capture: failed to bind managers");
		client_disconnect(&conn);
		return 1;
	}

	wl_display_roundtrip(conn.display);

	ext_image_copy_capture_manager_v1_destroy(cap_mgr);
	ext_output_image_capture_source_manager_v1_destroy(src_mgr);
	client_disconnect(&conn);
	return 0;
}
