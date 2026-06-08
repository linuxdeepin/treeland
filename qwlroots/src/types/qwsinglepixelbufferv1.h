// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(single_pixel_buffer_manager_v1)
{
public:
    QW_FUNC_STATIC(single_pixel_buffer_manager_v1, create, qw_single_pixel_buffer_manager_v1 *, wl_display *display)
};

QW_END_NAMESPACE
