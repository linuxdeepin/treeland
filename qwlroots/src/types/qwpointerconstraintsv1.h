// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_pointer_constraints_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(pointer_constraint_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(set_region)

public:
    QW_FUNC_MEMBER(pointer_constraint_v1, send_activated, void)
    QW_FUNC_MEMBER(pointer_constraint_v1, send_deactivated, void)
};

class QW_CLASS_OBJECT(pointer_constraints_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_constraint, wlr_pointer_constraint_v1*)

public:
    QW_FUNC_STATIC(pointer_constraints_v1, create, qw_pointer_constraints_v1 *, wl_display *display)

    QW_FUNC_MEMBER(pointer_constraints_v1, constraint_for_surface, wlr_pointer_constraint_v1 *, wlr_surface *surface, wlr_seat *seat)
};

QW_END_NAMESPACE
