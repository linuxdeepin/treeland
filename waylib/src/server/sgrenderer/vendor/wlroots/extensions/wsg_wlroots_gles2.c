// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension implementations for vendored wlroots gles2 renderer.
// These thin wrappers expose internal struct fields for BAL gles2 backend use.

#include "wsg_wlroots_gles2.h"

GLuint wsg_wlroots_gles2_buffer_get_fbo(struct wlr_gles2_buffer *buffer) {
    return buffer->fbo;
}

GLuint wsg_wlroots_gles2_buffer_get_rbo(struct wlr_gles2_buffer *buffer) {
    return buffer->rbo;
}

void *wsg_wlroots_gles2_buffer_get_egl_image(struct wlr_gles2_buffer *buffer) {
    return (void *)(uintptr_t)buffer->image;
}

GLuint wsg_wlroots_gles2_buffer_get_tex(struct wlr_gles2_buffer *buffer) {
    return buffer->tex;
}

bool wsg_wlroots_gles2_buffer_is_external_only(struct wlr_gles2_buffer *buffer) {
    return buffer->external_only;
}

GLuint wsg_wlroots_gles2_texture_get_gl_tex(struct wlr_gles2_texture *texture) {
    return texture->tex;
}

GLenum wsg_wlroots_gles2_texture_get_target(struct wlr_gles2_texture *texture) {
    return texture->target;
}

bool wsg_wlroots_gles2_texture_has_alpha(struct wlr_gles2_texture *texture) {
    return texture->has_alpha;
}

GLuint wsg_wlroots_gles2_texture_get_fbo(struct wlr_gles2_texture *texture) {
    return texture->fbo;
}

bool wsg_wlroots_gles2_make_current(struct wlr_gles2_renderer *renderer,
    struct wlr_egl_context *save_context) {
    return wlr_egl_make_current(renderer->egl, save_context);
}

void wsg_wlroots_gles2_restore_context(struct wlr_egl_context *prev_ctx) {
    wlr_egl_restore_context(prev_ctx);
}

struct wlr_egl *wsg_wlroots_gles2_renderer_get_egl(struct wlr_gles2_renderer *renderer) {
    return renderer->egl;
}
