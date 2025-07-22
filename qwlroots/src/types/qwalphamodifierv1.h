// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/types/wlr_alpha_modifier_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(alpha_modifier_v1)
{
public:
    QW_FUNC_STATIC(alpha_modifier_v1, create, qw_alpha_modifier_v1 *, wl_display *display)
    QW_FUNC_STATIC(alpha_modifier_v1, get_surface_state, const wlr_alpha_modifier_surface_v1_state *, wlr_surface *surface)
};

QW_END_NAMESPACE
