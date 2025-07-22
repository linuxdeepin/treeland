// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
// avoid replace static
#include <wayland-server-core.h>
#define static
#include <wlr/types/wlr_xdg_decoration_v1.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xdg_decoration_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_toplevel_decoration, wlr_xdg_toplevel_decoration_v1*)

public:
    QW_FUNC_STATIC(xdg_decoration_manager_v1, create, qw_xdg_decoration_manager_v1 *, wl_display *display)
};

class QW_CLASS_OBJECT(xdg_toplevel_decoration_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(request_mode)

public:
    QW_FUNC_MEMBER(xdg_toplevel_decoration_v1, set_mode, uint32_t, enum wlr_xdg_toplevel_decoration_v1_mode mode)
};

QW_END_NAMESPACE
