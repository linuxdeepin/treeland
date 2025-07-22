// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

#include <qwseat.h>

extern "C" {
#define static
#include <wlr/types/wlr_transient_seat_v1.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(transient_seat_v1)
{
public:
    QW_FUNC_MEMBER(transient_seat_v1, ready, void, wlr_seat *seat)
    QW_FUNC_MEMBER(transient_seat_v1, deny, void)
};

class QW_CLASS_OBJECT(transient_seat_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(create_seat, wlr_transient_seat_v1*)

public:
    QW_FUNC_STATIC(transient_seat_manager_v1, create, qw_transient_seat_manager_v1 *, wl_display *display)
};

QW_END_NAMESPACE
