// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_cursor_shape_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(cursor_shape_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(request_set_shape, wlr_cursor_shape_manager_v1_request_set_shape_event*)

public:
    QW_FUNC_STATIC(cursor_shape_manager_v1, create, qw_cursor_shape_manager_v1 *, wl_display *display, uint32_t version)
    QW_FUNC_STATIC(cursor_shape_v1, name, const char *, enum wp_cursor_shape_device_v1_shape shape)
};

QW_END_NAMESPACE

