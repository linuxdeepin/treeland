// Copyright (C) 2025 <UnionTech Software Technology Co., Ltd.>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_ext_data_control_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(ext_data_control_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_device, wlr_ext_data_control_device_v1*)

public:
    QW_FUNC_STATIC(ext_data_control_manager_v1, create, qw_ext_data_control_manager_v1 *, wl_display *display, uint32_t version)
};

QW_END_NAMESPACE

