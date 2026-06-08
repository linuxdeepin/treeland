// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_idle_inhibit_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(idle_inhibit_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_inhibitor, wlr_idle_inhibitor_v1*)

public:
    QW_FUNC_STATIC(idle_inhibit_v1, create, qw_idle_inhibit_manager_v1 *, wl_display *display)
};

class QW_CLASS_OBJECT(idle_inhibitor_v1)
{
    QW_OBJECT
    Q_OBJECT
};

QW_END_NAMESPACE

