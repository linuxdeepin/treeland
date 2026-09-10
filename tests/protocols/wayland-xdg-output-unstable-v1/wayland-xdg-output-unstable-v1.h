/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_XDG_OUTPUT_TEST_H
#define WAYLAND_XDG_OUTPUT_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real headless WOutput's layout geometry, read on the
 * compositor (Qt) thread and compared against the logical_position /
 * logical_size events the client received from zxdg_output_v1.
 *
 * xdg-output sends logical_position = wlr_output_layout_output->{x,y} and
 * logical_size = wlr_output_effective_resolution(); WOutput::position() and
 * WOutput::effectiveSize() read exactly those same wlroots values, so an
 * end-to-end (E-level) test cross-checks the protocol events against the live
 * production output object instead of asserting hard-coded numbers.
 */
struct xdg_output_server_state {
	int valid; /* a real WOutput was found in the layout */
	int32_t x; /* WOutput::position().x()  == layout x */
	int32_t y; /* WOutput::position().y()  == layout y */
	int32_t width; /* WOutput::effectiveSize().width()  */
	int32_t height; /* WOutput::effectiveSize().height() */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the real output geometry. */
void xdg_output_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_XDG_OUTPUT_TEST_H */
