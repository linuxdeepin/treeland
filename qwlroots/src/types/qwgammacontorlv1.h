// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include<qwobject.h>

extern "C" {
#include <wlr/types/wlr_gamma_control_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(gamma_control_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(set_gamma, wlr_gamma_control_manager_v1_set_gamma_event*)

public:
    QW_FUNC_STATIC(gamma_control_manager_v1, create, qw_gamma_control_manager_v1 *, wl_display *display)

    QW_FUNC_MEMBER(gamma_control_manager_v1, get_control, wlr_gamma_control_v1 *, wlr_output *output)
};

class QW_CLASS_REINTERPRET_CAST(gamma_control_v1)
{
public:
    QW_FUNC_MEMBER(gamma_control_v1, apply, bool, wlr_output_state *output_state)
    QW_FUNC_MEMBER(gamma_control_v1, send_failed_and_destroy, void)
};

QW_END_NAMESPACE
