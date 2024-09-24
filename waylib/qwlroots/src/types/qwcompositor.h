// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

struct wlr_subsurface;
struct wlr_surface_state;

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(compositor)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_surface, wlr_surface*)

public:
    QW_FUNC_STATIC(compositor, create, qw_compositor *, wl_display *display, uint32_t version, wlr_renderer *renderer)
};

class QW_CLASS_OBJECT(surface)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(client_commit)
    QW_SIGNAL(commit)
    QW_SIGNAL(new_subsurface, wlr_subsurface*)
#if WLR_VERSION_MINOR < 18
    QW_SIGNAL(precommit, wlr_surface_state*)
#endif
    QW_SIGNAL(map)
    QW_SIGNAL(unmap)

public:
    QW_FUNC_STATIC(surface, from_resource, qw_surface *, wl_resource *resource)

    QW_FUNC_MEMBER(surface, for_each_surface, void, wlr_surface_iterator_func_t iterator, void *user_data)
    QW_FUNC_MEMBER(surface, get_buffer_source_box, void, wlr_fbox *box)
    QW_FUNC_MEMBER(surface, get_effective_damage, void, pixman_region32_t *damage)
    QW_FUNC_MEMBER(surface, get_extends, void, wlr_box *box)
    QW_FUNC_MEMBER(surface, get_root_surface, wlr_surface *)
    QW_FUNC_MEMBER(surface, get_texture, wlr_texture *)
    QW_FUNC_MEMBER(surface, has_buffer, bool)
    QW_FUNC_MEMBER(surface, lock_pending, uint32_t)
    QW_FUNC_MEMBER(surface, point_accepts_input, bool, double sx, double sy)
    QW_FUNC_MEMBER(surface, send_enter, void, wlr_output *output)
    QW_FUNC_MEMBER(surface, send_frame_done, void, const timespec *when)
    QW_FUNC_MEMBER(surface, send_leave, void, wlr_output *output)
    QW_FUNC_MEMBER(surface, surface_at, wlr_surface *, double sx, double sy, double *sub_x, double *sub_y)
    QW_FUNC_MEMBER(surface, unlock_cached, void, uint32_t seq)
    QW_FUNC_MEMBER(surface, set_preferred_buffer_scale, void, int32_t scale)
    QW_FUNC_MEMBER(surface, set_preferred_buffer_transform, void, enum wl_output_transform transform)
    QW_FUNC_MEMBER(surface, set_role, bool, const wlr_surface_role *role, wl_resource *error_resource, uint32_t error_code)
    QW_FUNC_MEMBER(surface, map, void)
    QW_FUNC_MEMBER(surface, unmap, void)
};

QW_END_NAMESPACE
