/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_ALPHA_MODIFIER_TEST_H
#define WAYLAND_ALPHA_MODIFIER_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real mapped SurfaceWrapper's alpha-modifier state, read on
 * the compositor (Qt) thread.  wp_alpha_modifier_surface_v1.set_multiplier is
 * double-buffered state applied on wl_surface.commit.  The wlroots struct
 * stores the multiplier as a double in [0, 1], where 0 is fully transparent
 * and 1 is fully opaque (UINT32_MAX).  An E-level test verifies that the
 * production state reflects the client's requested multiplier.
 */
struct alpha_modifier_server_state {
	int valid; /* a mapped SurfaceWrapper was captured */
	int has_modifier; /* alpha modifier state found on the surface */
	double multiplier; /* wlr_alpha_modifier_surface_v1_state::multiplier */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the multiplier. */
void alpha_modifier_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_ALPHA_MODIFIER_TEST_H */
