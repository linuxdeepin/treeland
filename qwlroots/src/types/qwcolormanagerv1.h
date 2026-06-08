// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/types/wlr_color_management_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(color_manager_v1)
{
public:
    QW_FUNC_STATIC(color_manager_v1, create, qw_color_manager_v1 *, wl_display *display,
        uint32_t version, struct wlr_color_manager_v1_options *options)
    QW_FUNC_MEMBER(color_manager_v1, set_surface_preferred_image_description, void, wlr_surface *surface,
        const wlr_image_description_v1_data *data)
};

QW_END_NAMESPACE
