/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_CURSOR_SHAPE_TEST_H
#define WAYLAND_CURSOR_SHAPE_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WCursorShapeManagerV1 captures the
 * request_set_shape signal emitted by wlroots when the client calls
 * wp_cursor_shape_device_v1.set_shape.  The captured shape value must match
 * the client's requested shape, proving the cursor-shape request reached the
 * real compositor cursor pipeline rather than merely surviving without a
 * protocol error.
 */
struct cursor_shape_server_state {
	int valid; /* a shape request was captured */
	uint32_t shape; /* wp_cursor_shape_device_v1_shape enum value */
	int device_type; /* wlr_cursor_shape_manager_v1_device_type */
};

void cursor_shape_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_CURSOR_SHAPE_TEST_H */
