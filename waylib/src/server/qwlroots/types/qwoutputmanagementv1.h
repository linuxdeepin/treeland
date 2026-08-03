// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_output_management_v1.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(output_configuration_v1)
{
public:
    QW_FUNC_STATIC(output_configuration_v1, create, qw_output_configuration_v1 *, void)

    QW_FUNC_MEMBER(output_configuration_v1, send_failed, void)
    QW_FUNC_MEMBER(output_configuration_v1, send_succeeded, void)

protected:
    friend class qw_reinterpret_cast;
    QW_FUNC_MEMBER(output_configuration_v1, destroy, void)
};

class QW_CLASS_REINTERPRET_CAST(output_configuration_head_v1)
{
public:
    QW_FUNC_STATIC(output_configuration_head_v1, create, qw_output_configuration_head_v1 *, wlr_output_configuration_v1 *config, wlr_output *output)
};

class QW_CLASS_OBJECT(output_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(apply, wlr_output_configuration_v1 *)
    QW_SIGNAL(test, wlr_output_configuration_v1 *)

public:
    QW_FUNC_STATIC(output_manager_v1, create, qw_output_manager_v1 *, wl_display *display)
    QW_FUNC_MEMBER(output_manager_v1, set_configuration, void, wlr_output_configuration_v1 *config)
};

QW_END_NAMESPACE
