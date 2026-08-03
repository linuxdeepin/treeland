// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>
extern "C" {
#include <wlr/types/wlr_relative_pointer_v1.h>
}
QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(relative_pointer_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(relative_pointer_v1, from_resource, qw_relative_pointer_v1 *, wl_resource *resource)
};

class QW_CLASS_OBJECT(relative_pointer_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(relative_pointer_manager_v1, create, qw_relative_pointer_manager_v1 *, wl_display *display)

    QW_FUNC_MEMBER(relative_pointer_manager_v1, send_relative_motion, void, wlr_seat *seat, uint64_t time_usec, double dx, double dy, double dx_unaccel, double dy_unaccel)
};

QW_END_NAMESPACE
