// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_xdg_dialog_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xdg_dialog_v1)
{
    QW_OBJECT
    Q_OBJECT
 
    QW_SIGNAL(set_modal)

public:
    QW_FUNC_STATIC(xdg_dialog_v1, try_from_wlr_xdg_toplevel, qw_xdg_dialog_v1 *, wlr_xdg_toplevel *xdg_toplevel)
};

class QW_CLASS_OBJECT(xdg_wm_dialog_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_dialog, wlr_xdg_dialog_v1 *)

public:
    QW_FUNC_STATIC(xdg_wm_dialog_v1, create, qw_xdg_wm_dialog_v1 *, wl_display *display, uint32_t version)
};

QW_END_NAMESPACE
