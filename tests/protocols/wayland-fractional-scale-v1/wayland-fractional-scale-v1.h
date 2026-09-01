/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_FRACTIONAL_SCALE_TEST_H
#define WAYLAND_FRACTIONAL_SCALE_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real WOutput scale, read on the compositor (Qt) thread.
 * wlroots sends wp_fractional_scale_v1.preferred_scale as a numerator with a
 * denominator of 120.  The expected value is round(WOutput::scale() * 120).
 * An E-level test verifies that the client-side preferred_scale event matches
 * the live production output scale, not merely that an event was delivered.
 */
struct fractional_scale_server_state {
	int valid; /* a WOutput was found */
	float scale; /* WOutput::scale() */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the output's scale. */
void fractional_scale_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_FRACTIONAL_SCALE_TEST_H */
