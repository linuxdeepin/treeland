// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension header for vendored wlroots pixman renderer.
// Exposes internal buffer/texture APIs for use by the BAL layer.

#pragma once

#include <pixman.h>
#include <stdbool.h>

#include "render/wlr_pixman.h"

#ifdef __cplusplus
extern "C" {
#endif

// === Buffer → render target conversion ===

// Returns the pixman_image_t backing a pixman buffer.
// The pixman_image_t can be used directly as a render target by the BAL
// pixman backend (wrapped in a QImage for QPainter operations).
pixman_image_t *wsg_wlroots_pixman_buffer_get_image(struct wlr_pixman_buffer *buffer);

// === Texture handle access ===

// Returns the pixman_image_t of a pixman texture.
pixman_image_t *wsg_wlroots_pixman_texture_get_image(struct wlr_pixman_texture *texture);

#ifdef __cplusplus
}
#endif
