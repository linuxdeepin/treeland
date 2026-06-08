// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/types/wlr_idle_notify_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(idle_notifier_v1)
{
public:
    QW_FUNC_STATIC(idle_notifier_v1, create, qw_idle_notifier_v1 *, wl_display *display)

    QW_FUNC_MEMBER(idle_notifier_v1, notify_activity, void, wlr_seat *seat)
    QW_FUNC_MEMBER(idle_notifier_v1, set_inhibited, void, bool inhibited)
};

QW_END_NAMESPACE
