// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define class _class
#include <wlr/xwayland/xwayland.h>
#undef class
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xwayland)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(ready)
    QW_SIGNAL(new_surface, wlr_xwayland_surface*)
    QW_SIGNAL(remove_startup_info, wlr_xwayland_remove_startup_info_event*)

public:
    QW_FUNC_STATIC(xwayland, create, qw_xwayland *, wl_display *wl_display, wlr_compositor *compositor, bool lazy)

    QW_FUNC_MEMBER(xwayland, set_cursor, void, uint8_t *pixels, uint32_t stride, uint32_t width, uint32_t height, int32_t hotspot_x, int32_t hotspot_y)
    QW_FUNC_MEMBER(xwayland, set_seat, void, wlr_seat *seat)
};

QW_END_NAMESPACE
