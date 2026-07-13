// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension header for vendored wlroots gles2 renderer.
// Exposes internal buffer/texture/context APIs for use by the BAL layer.

#pragma once

#include <GLES2/gl2.h>
#include <stdbool.h>

#include "render/gles2.h"
#include "render/egl.h"

#ifdef __cplusplus
extern "C" {
#endif

// === Buffer -> render target conversion ===

GLuint wsg_wlroots_gles2_buffer_get_fbo(struct wlr_gles2_buffer *buffer);
GLuint wsg_wlroots_gles2_buffer_get_rbo(struct wlr_gles2_buffer *buffer);
void *wsg_wlroots_gles2_buffer_get_egl_image(struct wlr_gles2_buffer *buffer);
GLuint wsg_wlroots_gles2_buffer_get_tex(struct wlr_gles2_buffer *buffer);
bool wsg_wlroots_gles2_buffer_is_external_only(struct wlr_gles2_buffer *buffer);

// === Texture handle access ===

GLuint wsg_wlroots_gles2_texture_get_gl_tex(struct wlr_gles2_texture *texture);
GLenum wsg_wlroots_gles2_texture_get_target(struct wlr_gles2_texture *texture);
bool wsg_wlroots_gles2_texture_has_alpha(struct wlr_gles2_texture *texture);
GLuint wsg_wlroots_gles2_texture_get_fbo(struct wlr_gles2_texture *texture);

// === EGL context management ===

// Makes the gles2 renderer's EGL context current. Pass a wlr_egl_context*
// to save the previous context for later restoration.
bool wsg_wlroots_gles2_make_current(struct wlr_gles2_renderer *renderer,
    struct wlr_egl_context *save_context);

// Restores the EGL context saved by a prior wsg_wlroots_gles2_make_current().
void wsg_wlroots_gles2_restore_context(struct wlr_egl_context *prev_ctx);

// Returns the wlr_egl handle from a gles2 renderer.
struct wlr_egl *wsg_wlroots_gles2_renderer_get_egl(struct wlr_gles2_renderer *renderer);

#ifdef __cplusplus
}
#endif
