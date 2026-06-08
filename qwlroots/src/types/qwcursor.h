// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_cursor.h>
#undef static
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/box.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(cursor)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(motion, wlr_pointer_motion_event*)
    QW_SIGNAL(motion_absolute, wlr_pointer_motion_absolute_event*)
    QW_SIGNAL(button, wlr_pointer_button_event*)
    QW_SIGNAL(axis, wlr_pointer_axis_event*)
    QW_SIGNAL(frame)
    QW_SIGNAL(swipe_begin, wlr_pointer_swipe_begin_event*)
    QW_SIGNAL(swipe_update, wlr_pointer_swipe_update_event*)
    QW_SIGNAL(swipe_end, wlr_pointer_swipe_end_event*)
    QW_SIGNAL(pinch_begin, wlr_pointer_pinch_begin_event*)
    QW_SIGNAL(pinch_update, wlr_pointer_pinch_update_event*)
    QW_SIGNAL(pinch_end, wlr_pointer_pinch_end_event*)
    QW_SIGNAL(hold_begin, wlr_pointer_hold_begin_event*)
    QW_SIGNAL(hold_end, wlr_pointer_hold_end_event*)
    QW_SIGNAL(touch_up, wlr_touch_up_event*)
    QW_SIGNAL(touch_down, wlr_touch_down_event*)
    QW_SIGNAL(touch_motion, wlr_touch_motion_event*)
    QW_SIGNAL(touch_cancel, wlr_touch_cancel_event*)
    QW_SIGNAL(touch_frame)
    QW_SIGNAL(tablet_tool_axis, wlr_tablet_tool_axis_event*)
    QW_SIGNAL(tablet_tool_proximity, wlr_tablet_tool_proximity_event*)
    QW_SIGNAL(tablet_tool_tip, wlr_tablet_tool_tip_event*)
    QW_SIGNAL(tablet_tool_button, wlr_tablet_tool_button_event*)

public:
    QW_FUNC_STATIC(cursor, create, qw_cursor *, void)

    QW_FUNC_MEMBER(cursor, warp, bool, wlr_input_device *dev, double lx, double ly)
    QW_FUNC_MEMBER(cursor, warp_closest, void, wlr_input_device *dev, double x, double y)
    QW_FUNC_MEMBER(cursor, warp_absolute, void, wlr_input_device *dev, double x, double y)
    QW_FUNC_MEMBER(cursor, move, void, wlr_input_device *dev, double delta_x, double delta_y)
    QW_FUNC_MEMBER(cursor, set_buffer, void, wlr_buffer *buffer, int32_t hotspot_x, int32_t hotspot_y, float scale)
    QW_FUNC_MEMBER(cursor, set_xcursor, void, wlr_xcursor_manager *manager, const char *name)
    QW_FUNC_MEMBER(cursor, unset_image, void)
    QW_FUNC_MEMBER(cursor, set_surface, void, wlr_surface *surface, int32_t hotspot_x, int32_t hotspot_y)
    QW_FUNC_MEMBER(cursor, attach_input_device, void, wlr_input_device *dev)
    QW_FUNC_MEMBER(cursor, detach_input_device, void, wlr_input_device *dev)
    QW_FUNC_MEMBER(cursor, attach_output_layout, void, wlr_output_layout *l)
    QW_FUNC_MEMBER(cursor, map_to_output, void, wlr_output *output)
    QW_FUNC_MEMBER(cursor, map_input_to_output, void, wlr_input_device *dev, wlr_output *output)
    QW_FUNC_MEMBER(cursor, map_to_region, void, const wlr_box *box)
    QW_FUNC_MEMBER(cursor, map_input_to_region, void, wlr_input_device *dev, const wlr_box *box)
    QW_FUNC_MEMBER(cursor, absolute_to_layout_coords, void, wlr_input_device *dev, double x, double y, double *lx, double *ly)

protected:
    QW_FUNC_MEMBER(cursor, destroy, void)
};

QW_END_NAMESPACE
