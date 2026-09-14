/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_VIEWPORTER_TEST_H
#define WAYLAND_VIEWPORTER_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real mapped SurfaceWrapper's wl_surface viewport state,
 * read on the compositor (Qt) thread.  wp_viewport.set_destination is
 * double-buffered state applied on wl_surface.commit.  An E-level test
 * verifies that the production wlr_surface current viewport has the expected
 * destination dimensions, proving the request reached the real compositor
 * surface pipeline rather than merely surviving without a protocol error.
 */
struct viewporter_server_state {
	int valid; /* a mapped SurfaceWrapper was captured */
	int has_dst; /* wlr_surface::current.viewport.has_dst */
	int dst_width; /* wlr_surface::current.viewport.dst_width */
	int dst_height; /* wlr_surface::current.viewport.dst_height */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the viewport state. */
void viewporter_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_VIEWPORTER_TEST_H */
