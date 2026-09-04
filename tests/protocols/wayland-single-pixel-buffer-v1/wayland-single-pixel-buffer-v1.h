/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_SINGLE_PIXEL_BUFFER_TEST_H
#define WAYLAND_SINGLE_PIXEL_BUFFER_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real mapped SurfaceWrapper's wl_surface buffer dimensions,
 * read on the compositor (Qt) thread.  A single-pixel buffer is 1×1, so an
 * E-level test verifies that the production wlr_surface current state reports
 * buffer_width == 1 and buffer_height == 1 after the buffer is committed,
 * proving the buffer was accepted by the real compositor surface pipeline.
 */
struct single_pixel_buffer_server_state {
	int valid; /* a mapped SurfaceWrapper was captured */
	int mapped; /* WSurface::mapped() */
	int buffer_width; /* wlr_surface::current.buffer_width */
	int buffer_height; /* wlr_surface::current.buffer_height */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the buffer dimensions. */
void single_pixel_buffer_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_SINGLE_PIXEL_BUFFER_TEST_H */
