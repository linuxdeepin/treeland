// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_session_lock_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(session_lock_surface_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(session_lock_surface_v1, try_from_wlr_surface, qw_session_lock_surface_v1 *, wlr_surface *surface)

    QW_FUNC_MEMBER(session_lock_surface_v1, configure, uint32_t, uint32_t width, uint32_t height)
};

class QW_CLASS_OBJECT(session_lock_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_surface, wlr_session_lock_surface_v1*)
    QW_SIGNAL(unlock)

public:
    QW_FUNC_MEMBER(session_lock_v1, send_locked, void)
    QW_FUNC_MEMBER(session_lock_v1, destroy, void)
};

class QW_CLASS_OBJECT(session_lock_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_lock, wlr_session_lock_v1*)

public:
    QW_FUNC_STATIC(session_lock_manager_v1, create, qw_session_lock_manager_v1 *, wl_display *display)
};

QW_END_NAMESPACE
