// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_xdg_output_v1.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xdg_output_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(xdg_output_manager_v1, create, qw_xdg_output_manager_v1 *, wl_display *display, wlr_output_layout *layout)
};

QW_END_NAMESPACE

