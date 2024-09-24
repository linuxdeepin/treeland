// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wayland-server-core.h>
#define static
#include <wlr/render/wlr_renderer.h>
#if WLR_VERSION_MINOR >= 18
#include <wlr/render/pass.h>
#endif
#undef static
#include <wlr/util/box.h>
}

struct wlr_renderer;
struct wlr_box;
struct wlr_fbox;
struct wlr_drm_format_set;
struct wl_display;
struct wlr_render_texture_options;

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(renderer)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(lost)

public:
    QW_FUNC_STATIC(renderer, autocreate, qw_renderer *, wlr_backend *backend)

    QW_FUNC_MEMBER(renderer, init_wl_display, bool, wl_display *wl_display)
    QW_FUNC_MEMBER(renderer, init_wl_shm, bool, wl_display *wl_display)
    QW_FUNC_MEMBER(renderer, get_drm_fd, int)

#if WLR_VERSION_MAJOR == 0 && WLR_VERSION_MINOR < 18
    QW_FUNC_MEMBER(renderer, begin, bool, uint32_t width, uint32_t height)
    QW_FUNC_MEMBER(renderer, end, void)
    QW_FUNC_MEMBER(renderer, begin_buffer_pass, wlr_render_pass *, wlr_buffer *buffer, const wlr_buffer_pass_options *options)
    QW_FUNC_MEMBER(renderer, begin_with_buffer, bool, wlr_buffer *buffer)
    QW_FUNC_MEMBER(renderer, clear, void, const float color[4])
    QW_FUNC_MEMBER(renderer, scissor, void, wlr_box *box)
    QW_FUNC_MEMBER(render, texture, bool, wlr_texture *texture, const float projection[9], int x, int y, float alpha)
    QW_FUNC_MEMBER(render, texture_with_matrix, bool, wlr_texture *texture, const float matrix[9], float alpha)
    QW_FUNC_MEMBER(render, subtexture_with_matrix, bool, wlr_texture *texture, const wlr_fbox *box, const float matrix[9], float alpha)
    QW_FUNC_MEMBER(render, rect, void, const wlr_box *box, const float color[4], const float projection[9])
    QW_FUNC_MEMBER(render, quad_with_matrix, void, const float color[4], const float matrix[9])
    QW_FUNC_MEMBER(renderer, get_shm_texture_formats, const uint32_t *, size_t *len)
    QW_FUNC_MEMBER(renderer, get_dmabuf_texture_formats, const wlr_drm_format_set *)
    QW_FUNC_MEMBER(renderer, read_pixels, bool, uint32_t fmt, uint32_t stride, uint32_t width, uint32_t height, uint32_t src_x, uint32_t src_y, uint32_t dst_x, uint32_t dst_y, void *data)
#else // WLR_VERSION_MINOR >= 18
    QW_FUNC_MEMBER(renderer, get_texture_formats, const wlr_drm_format_set *, uint32_t buffer_caps)
#endif

protected:
    QW_FUNC_MEMBER(renderer, destroy, void)
};

QW_END_NAMESPACE
