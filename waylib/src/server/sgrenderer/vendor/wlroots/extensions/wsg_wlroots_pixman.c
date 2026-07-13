// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension implementations for vendored wlroots pixman renderer.
// These thin wrappers expose internal struct fields for BAL pixman backend use.

#include "wsg_wlroots_pixman.h"

pixman_image_t *wsg_wlroots_pixman_buffer_get_image(struct wlr_pixman_buffer *buffer) {
    return buffer->image;
}

pixman_image_t *wsg_wlroots_pixman_texture_get_image(struct wlr_pixman_texture *texture) {
    return texture->image;
}
