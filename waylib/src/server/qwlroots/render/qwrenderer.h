// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wayland-server-core.h>
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/pass.h>
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
    QW_FUNC_MEMBER(renderer, get_texture_formats, const wlr_drm_format_set *, uint32_t buffer_caps)

protected:
    QW_FUNC_MEMBER(renderer, destroy, void)
};

QW_END_NAMESPACE
