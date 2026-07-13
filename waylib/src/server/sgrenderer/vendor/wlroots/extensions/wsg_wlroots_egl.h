// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension header for vendored wlroots EGL layer.
// Exposes EGL context management and dmabuf conversion APIs for use by the BAL.

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdbool.h>

#include "render/egl.h"

#ifdef __cplusplus
extern "C" {
#endif

// === EGL context management ===

bool wsg_wlroots_egl_make_current(struct wlr_egl *egl,
    struct wlr_egl_context *save_context);
void wsg_wlroots_egl_restore_context(struct wlr_egl_context *context);

EGLDisplay wsg_wlroots_egl_get_display(struct wlr_egl *egl);
EGLContext wsg_wlroots_egl_get_context(struct wlr_egl *egl);
EGLSurface wsg_wlroots_egl_context_get_draw_surface(struct wlr_egl_context *ctx);
EGLSurface wsg_wlroots_egl_context_get_read_surface(struct wlr_egl_context *ctx);

// === dmabuf -> EGLImage conversion ===

EGLImageKHR wsg_wlroots_egl_create_image_from_dmabuf(struct wlr_egl *egl,
    struct wlr_dmabuf_attributes *attributes, bool *external_only);
bool wsg_wlroots_egl_destroy_image(struct wlr_egl *egl, EGLImageKHR image);

#ifdef __cplusplus
}
#endif
