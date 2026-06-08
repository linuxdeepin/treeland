// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define namespace scope
#define static
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#undef static
#include <wlr/types/wlr_xdg_shell.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(layer_surface_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_popup, wlr_xdg_popup*)

public:
    QW_FUNC_STATIC(layer_surface_v1, try_from_wlr_surface, qw_layer_surface_v1 *, wlr_surface *surface)

    QW_FUNC_MEMBER(layer_surface_v1, for_each_surface, void, wlr_surface_iterator_func_t iterator, void *user_data)
    QW_FUNC_MEMBER(layer_surface_v1, for_each_popup_surface, void, wlr_surface_iterator_func_t iterator, void *user_data)
    QW_FUNC_MEMBER(layer_surface_v1, surface_at, wlr_surface *, double sx, double sy, double *sub_x, double *sub_y)
    QW_FUNC_MEMBER(layer_surface_v1, popup_surface_at, wlr_surface *, double sx, double sy, double *sub_x, double *sub_y)
    QW_FUNC_MEMBER(layer_surface_v1, configure, uint32_t, uint32_t width, uint32_t height);
    QW_FUNC_MEMBER(layer_surface_v1, destroy, void)
};

class QW_CLASS_OBJECT(layer_shell_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_surface, wlr_layer_surface_v1 *surface)

public:
    QW_FUNC_STATIC(layer_shell_v1, create, qw_layer_shell_v1 *, wl_display *display, uint32_t version)
};

QW_END_NAMESPACE
