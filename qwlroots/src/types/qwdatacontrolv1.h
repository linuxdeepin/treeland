// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_data_control_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(data_control_device_v1)
{
public:
    QW_FUNC_MEMBER(data_control_device_v1, destroy, void)
};

class QW_CLASS_OBJECT(data_control_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_device, wlr_data_control_device_v1*)

public:
    QW_FUNC_STATIC(data_control_manager_v1, create, qw_data_control_manager_v1 *, wl_display *display);
};

QW_END_NAMESPACE

