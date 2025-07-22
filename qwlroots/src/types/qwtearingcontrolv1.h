// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_tearing_control_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(tearing_control_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_object, wlr_tearing_control_v1*)

public:
    QW_FUNC_STATIC(tearing_control_manager_v1, create, qw_tearing_control_manager_v1 *, wl_display *display, uint32_t version)

    QW_FUNC_MEMBER(tearing_control_manager_v1, surface_hint_from_surface, enum wp_tearing_control_v1_presentation_hint, wlr_surface *surface)
};

QW_END_NAMESPACE
