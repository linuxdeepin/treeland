// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xdg_shell)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_surface, wlr_xdg_surface*)
#if WLR_VERSION_MINOR >= 18
    QW_SIGNAL(new_toplevel, wlr_xdg_toplevel*)
    QW_SIGNAL(new_popup, wlr_xdg_popup*)
#endif

public:
    QW_FUNC_STATIC(xdg_shell, create, qw_xdg_shell *, wl_display *display, uint32_t version)
};

class QW_CLASS_OBJECT(xdg_surface)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(ping_timeout)
    QW_SIGNAL(new_popup, wlr_xdg_popup*)
    QW_SIGNAL(configure, wlr_xdg_surface_configure*)
    QW_SIGNAL(ack_configure, wlr_xdg_surface_configure*)

public:
    QW_FUNC_STATIC(xdg_surface, from_resource, qw_xdg_surface *, wl_resource *resource)
    QW_FUNC_STATIC(xdg_surface, try_from_wlr_surface, qw_xdg_surface *, wlr_surface *surface)

    QW_FUNC_MEMBER(xdg_surface, surface_at, wlr_surface *, double sx, double sy, double *sub_x, double *sub_y)
    QW_FUNC_MEMBER(xdg_surface, popup_surface_at, wlr_surface *, double sx, double sy, double *sub_x, double *sub_y)
    QW_FUNC_MEMBER(xdg_surface, get_geometry, void, wlr_box *box)
    QW_FUNC_MEMBER(xdg_surface, for_each_surface, void, wlr_surface_iterator_func_t iterator, void *user_data)
    QW_FUNC_MEMBER(xdg_surface, for_each_popup_surface, void, wlr_surface_iterator_func_t iterator, void *user_data)
    QW_FUNC_MEMBER(xdg_surface, schedule_configure, uint32_t)
};

class QW_CLASS_OBJECT(xdg_popup)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(reposition)

public:
    QW_FUNC_STATIC(xdg_popup, from_resource, qw_xdg_popup *, wl_resource *resource)
    QW_FUNC_STATIC(xdg_popup, try_from_wlr_surface, qw_xdg_popup *, wlr_surface *surface)

    QW_FUNC_MEMBER(xdg_popup, get_position, void, double *popup_sx, double *popup_sy)
    QW_FUNC_MEMBER(xdg_popup, get_toplevel_coords, void, int popup_sx, int popup_sy, int *toplevel_sx, int *toplevel_sy)
    QW_FUNC_MEMBER(xdg_popup, unconstrain_from_box, void, const wlr_box *toplevel_space_box)
    QW_FUNC_MEMBER(xdg_popup, destroy, void)
};

class QW_CLASS_OBJECT(xdg_toplevel)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(request_maximize)
    QW_SIGNAL(request_fullscreen)
    QW_SIGNAL(request_minimize)
    QW_SIGNAL(request_move, wlr_xdg_toplevel_move_event*)
    QW_SIGNAL(request_resize, wlr_xdg_toplevel_resize_event*)
    QW_SIGNAL(request_show_window_menu, wlr_xdg_toplevel_show_window_menu_event*)
    QW_SIGNAL(set_parent, wlr_xdg_toplevel*)
    QW_SIGNAL(set_title, char*)
    QW_SIGNAL(set_app_id, char*)

public:
    QW_FUNC_STATIC(xdg_toplevel, from_resource, qw_xdg_toplevel *, wl_resource *resource)
    QW_FUNC_STATIC(xdg_toplevel, try_from_wlr_surface, qw_xdg_toplevel *, wlr_surface *surface)

    QW_FUNC_MEMBER(xdg_toplevel, set_size, uint32_t, int32_t width, int32_t height)
    QW_FUNC_MEMBER(xdg_toplevel, set_activated, uint32_t, bool activated)
    QW_FUNC_MEMBER(xdg_toplevel, set_maximized, uint32_t, bool maximized)
    QW_FUNC_MEMBER(xdg_toplevel, set_fullscreen, uint32_t, bool fullscreen)
    QW_FUNC_MEMBER(xdg_toplevel, set_resizing, uint32_t, bool resizing)
    QW_FUNC_MEMBER(xdg_toplevel, set_tiled, uint32_t, uint32_t tiled_edges)
    QW_FUNC_MEMBER(xdg_toplevel, set_bounds, uint32_t, int32_t width, int32_t height)
    QW_FUNC_MEMBER(xdg_toplevel, set_wm_capabilities, uint32_t, uint32_t caps)
    QW_FUNC_MEMBER(xdg_toplevel, send_close, void)
    QW_FUNC_MEMBER(xdg_toplevel, set_parent, bool, wlr_xdg_toplevel *parent)
};

QW_END_NAMESPACE

