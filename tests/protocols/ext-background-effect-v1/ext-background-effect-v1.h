/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef EXT_BACKGROUND_EFFECT_TEST_H
#define EXT_BACKGROUND_EFFECT_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real mapped SurfaceWrapper's ext-background-effect-v1 blur
 * state, read on the compositor (Qt) thread.  The blur region is
 * double-buffered state applied on wl_surface.commit.
 */
struct background_effect_server_state {
	int valid; /* a mapped SurfaceWrapper was captured */
	int has_region; /* the surface has a non-empty committed blur region */
	int blur_enabled; /* SurfaceWrapper::blur() (region or personalization) */
	int x;
	int y;
	int width;
	int height; /* bounding rect of the committed blur region */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the blur region state. */
void background_effect_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* EXT_BACKGROUND_EFFECT_TEST_H */
