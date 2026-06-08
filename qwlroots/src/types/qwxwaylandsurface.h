// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR
// GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <xcb/xproto.h>
#include <math.h>
#define class _class
#include <wlr/xwayland/xwayland.h>
#undef class
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xwayland_surface)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(request_configure, wlr_xwayland_surface_configure_event*)
    QW_SIGNAL(request_move)
    QW_SIGNAL(request_resize, wlr_xwayland_resize_event*)
    QW_SIGNAL(request_minimize, wlr_xwayland_minimize_event*)
    QW_SIGNAL(request_maximize)
    QW_SIGNAL(request_fullscreen)
    QW_SIGNAL(request_activate)
    QW_SIGNAL(set_title)
    QW_SIGNAL(set_class)
    QW_SIGNAL(set_role)
    QW_SIGNAL(set_parent)
    QW_SIGNAL(set_startup_id)
    QW_SIGNAL(set_window_type)
    QW_SIGNAL(set_hints)
    QW_SIGNAL(set_decorations)
    QW_SIGNAL(set_override_redirect)
    QW_SIGNAL(set_geometry)
    QW_SIGNAL(ping_timeout)
    QW_SIGNAL(associate)
    QW_SIGNAL(dissociate)

public:
    QW_FUNC_STATIC(xwayland_surface, try_from_wlr_surface, qw_xwayland_surface *, wlr_surface *surface)

    QW_FUNC_MEMBER(xwayland_surface, activate, void, bool activated)
    QW_FUNC_MEMBER(xwayland_surface, restack, void, wlr_xwayland_surface *sibling, enum xcb_stack_mode_t mode)
    QW_FUNC_MEMBER(xwayland_surface, configure, void, int16_t x, int16_t y, uint16_t width, uint16_t height)
    QW_FUNC_MEMBER(xwayland_surface, close, void)
    QW_FUNC_MEMBER(xwayland_surface, set_withdrawn, void, bool withdrawn)
    QW_FUNC_MEMBER(xwayland_surface, set_minimized, void, bool minimized)
#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(xwayland_surface, set_maximized, void, bool maximized)
#else
    QW_FUNC_MEMBER(xwayland_surface, set_maximized, void, bool maximized_horz, bool maximized_vert)
#endif
    QW_FUNC_MEMBER(xwayland_surface, set_fullscreen, void, bool fullscreen)
    QW_FUNC_MEMBER(xwayland_surface, ping, void)
#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(xwayland, icccm_input_model, enum wlr_xwayland_icccm_input_model)
#else
    QW_FUNC_MEMBER(xwayland_surface, icccm_input_model, void)
#endif

#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(xwayland_or_surface, wants_focus, bool)
#else
    QW_FUNC_MEMBER(xwayland_surface_override_redirect, wants_focus, void)
#endif
};

QW_END_NAMESPACE
