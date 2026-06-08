// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_tablet_v2.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(tablet_manager_v2)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(tablet_v2, create, qw_tablet_manager_v2 *, wl_display *display)
};

class QW_CLASS_OBJECT(tablet_v2_tablet)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(tablet, create, qw_tablet_v2_tablet *, wlr_tablet_manager_v2 *manager, wlr_seat *wlr_seat, wlr_input_device *wlr_device)

    QW_FUNC_MEMBER(surface, accepts_tablet_v2, bool, wlr_surface *surface)
};

class QW_CLASS_OBJECT(tablet_v2_tablet_tool)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(set_cursor, wlr_tablet_v2_event_cursor*)

public:
    QW_FUNC_STATIC(tablet_tool, create, qw_tablet_v2_tablet_tool *, wlr_tablet_manager_v2 *manager, wlr_seat *wlr_seat, wlr_tablet_tool *wlr_tool)

    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, proximity_in, void, wlr_tablet_v2_tablet *tablet, wlr_surface *surface)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, down, void)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, motion, void, double x, double y)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, pressure, void, double pressure)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, distance, void, double distance)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, tilt, void, double x, double y)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, rotation, void, double degrees)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, slider, void, double position)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, wheel, void, double degrees, int32_t clicks)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, proximity_out, void)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_tool, button, void, uint32_t button, enum zwp_tablet_pad_v2_button_state state)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_proximity_in, void, wlr_tablet_v2_tablet *tablet, wlr_surface *surface)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_down, void)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_motion, void, double x, double y)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_pressure, void, double pressure)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_distance, void, double distance)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_tilt, void, double x, double y)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_rotation, void, double degrees)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_slider, void, double position)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_wheel, void, double degrees, int32_t clicks)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_proximity_out, void)
    QW_FUNC_MEMBER(tablet_v2_tablet_tool, notify_button, void, uint32_t button, enum zwp_tablet_pad_v2_button_state state)
    QW_FUNC_MEMBER(tablet_tool_v2, start_grab, void, wlr_tablet_tool_v2_grab *grab)
    QW_FUNC_MEMBER(tablet_tool_v2, end_grab, void)
    QW_FUNC_MEMBER(tablet_tool_v2, start_implicit_grab, void)
    QW_FUNC_MEMBER(tablet_tool_v2, has_implicit_grab, bool)
};

class QW_CLASS_OBJECT(tablet_v2_tablet_pad)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(button_feedback, wlr_tablet_v2_event_feedback*)
    QW_SIGNAL(strip_feedback, wlr_tablet_v2_event_feedback*)
    QW_SIGNAL(ring_feedback, wlr_tablet_v2_event_feedback*)

public:
    QW_FUNC_STATIC(tablet_pad, create, qw_tablet_v2_tablet_pad *, wlr_tablet_manager_v2 *manager, wlr_seat *wlr_seat, wlr_input_device *wlr_device)

    QW_FUNC_MEMBER(send_tablet_v2_tablet_pad, enter, uint32_t, wlr_tablet_v2_tablet *tablet, wlr_surface *surface)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_pad, button, void, size_t button, uint32_t time, enum zwp_tablet_pad_v2_button_state state)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_pad, strip, void, uint32_t strip, double position, bool finger, uint32_t time)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_pad, ring, void, uint32_t ring, double position, bool finger, uint32_t time)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_pad, leave, uint32_t, wlr_surface *surface)
    QW_FUNC_MEMBER(send_tablet_v2_tablet_pad, mode, uint32_t, size_t group, uint32_t mode, uint32_t time)
    QW_FUNC_MEMBER(tablet_v2_tablet_pad, notify_enter, uint32_t, wlr_tablet_v2_tablet *tablet, wlr_surface *surface)
    QW_FUNC_MEMBER(tablet_v2_tablet_pad, notify_button, void, size_t button, uint32_t time, enum zwp_tablet_pad_v2_button_state state)
    QW_FUNC_MEMBER(tablet_v2_tablet_pad, notify_strip, void, uint32_t strip, double position, bool finger, uint32_t time)
    QW_FUNC_MEMBER(tablet_v2_tablet_pad, notify_ring, void, uint32_t ring, double position, bool finger, uint32_t time)
    QW_FUNC_MEMBER(tablet_v2_tablet_pad, notify_leave, uint32_t, wlr_surface *surface)
    QW_FUNC_MEMBER(tablet_v2_tablet_pad, notify_mode, uint32_t, size_t group, uint32_t mode, uint32_t time)
    QW_FUNC_MEMBER(tablet_v2, end_grab, void)
    QW_FUNC_MEMBER(tablet_v2, start_grab, void, wlr_tablet_pad_v2_grab *grab)
};

QW_END_NAMESPACE
