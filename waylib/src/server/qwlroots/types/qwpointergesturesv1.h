// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_pointer_gestures_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(pointer_gestures_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(pointer_gestures_v1, create, qw_pointer_gestures_v1 *, wl_display *display)

    QW_FUNC_MEMBER(pointer_gestures_v1, send_swipe_begin, void, wlr_seat *seat, uint32_t time_msec, uint32_t fingers)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_swipe_update, void, wlr_seat *seat, uint32_t time_msec, double dx, double dy)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_swipe_end, void, wlr_seat *seat, uint32_t time_msec, bool cancelled)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_pinch_begin, void, wlr_seat *seat, uint32_t time_msec, uint32_t fingers)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_pinch_update, void, wlr_seat *seat, uint32_t time_msec, double dx, double dy, double scale, double rotation)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_pinch_end, void, wlr_seat *seat, uint32_t time_msec, bool cancelled)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_hold_begin, void, wlr_seat *seat, uint32_t time_msec, uint32_t fingers)
    QW_FUNC_MEMBER(pointer_gestures_v1, send_hold_end, void, wlr_seat *seat, uint32_t time_msec, bool cancelled)
};

QW_END_NAMESPACE

