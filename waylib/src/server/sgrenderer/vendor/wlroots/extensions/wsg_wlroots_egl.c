// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension implementations for vendored wlroots EGL layer.
// These thin wrappers expose internal EGL functions for BAL context management.

#include "wsg_wlroots_egl.h"

bool wsg_wlroots_egl_make_current(struct wlr_egl *egl,
    struct wlr_egl_context *save_context) {
    return wlr_egl_make_current(egl, save_context);
}

void wsg_wlroots_egl_restore_context(struct wlr_egl_context *context) {
    wlr_egl_restore_context(context);
}

EGLDisplay wsg_wlroots_egl_get_display(struct wlr_egl *egl) {
    return egl->display;
}

EGLContext wsg_wlroots_egl_get_context(struct wlr_egl *egl) {
    return egl->context;
}

EGLSurface wsg_wlroots_egl_context_get_draw_surface(struct wlr_egl_context *ctx) {
    return ctx->draw_surface;
}

EGLSurface wsg_wlroots_egl_context_get_read_surface(struct wlr_egl_context *ctx) {
    return ctx->read_surface;
}

EGLImageKHR wsg_wlroots_egl_create_image_from_dmabuf(struct wlr_egl *egl,
    struct wlr_dmabuf_attributes *attributes, bool *external_only) {
    return wlr_egl_create_image_from_dmabuf(egl, attributes, external_only);
}

bool wsg_wlroots_egl_destroy_image(struct wlr_egl *egl, EGLImageKHR image) {
    return wlr_egl_destroy_image(egl, image);
}
