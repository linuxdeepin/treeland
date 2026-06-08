// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include "math.h"
#define static
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(foreign_toplevel_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(foreign_toplevel_manager_v1, create, qw_foreign_toplevel_manager_v1 *, wl_display *display)
};

class QW_CLASS_OBJECT(foreign_toplevel_handle_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(request_maximize, wlr_foreign_toplevel_handle_v1_maximized_event*)
    QW_SIGNAL(request_minimize, wlr_foreign_toplevel_handle_v1_minimized_event*)
    QW_SIGNAL(request_activate, wlr_foreign_toplevel_handle_v1_activated_event*)
    QW_SIGNAL(request_fullscreen, wlr_foreign_toplevel_handle_v1_fullscreen_event*)
    QW_SIGNAL(request_close)
    QW_SIGNAL(set_rectangle, wlr_foreign_toplevel_handle_v1_set_rectangle_event*)

public:
    QW_FUNC_STATIC(foreign_toplevel_handle_v1, create, qw_foreign_toplevel_handle_v1 *, wlr_foreign_toplevel_manager_v1 *manager)

    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_title, void, const char *title)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_app_id, void, const char *app_id)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, output_enter, void, wlr_output *output)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, output_leave, void, wlr_output *output)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_maximized, void, bool maximized)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_minimized, void, bool minimized)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_activated, void, bool activated)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_fullscreen, void, bool fullscreen)
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, set_parent, void, wlr_foreign_toplevel_handle_v1 *parent)

protected:
    QW_FUNC_MEMBER(foreign_toplevel_handle_v1, destroy, void)
};

QW_END_NAMESPACE

