// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_data_device.h>
}


QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(data_source)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_MEMBER(data_source, init, void, const wlr_data_source_impl *impl)
    QW_FUNC_MEMBER(data_source, accept, void, uint32_t serial, const char *mime_type)
    QW_FUNC_MEMBER(data_source, send, void, const char *mime_type, int32_t fd)
    QW_FUNC_MEMBER(data_source, dnd_action, void, enum wl_data_device_manager_dnd_action action)
    QW_FUNC_MEMBER(data_source, dnd_drop, void)
    QW_FUNC_MEMBER(data_source, dnd_finish, void)

protected:
    QW_FUNC_MEMBER(data_source, destroy, void)
};

class QW_CLASS_OBJECT(data_device_manager)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(data_device_manager, create, qw_data_device_manager *, wl_display *display)
};

class QW_CLASS_OBJECT(drag)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(focus)
    QW_SIGNAL(motion, wlr_drag_motion_event*)
    QW_SIGNAL(drop, wlr_drag_drop_event*)

public:
    QW_FUNC_STATIC(drag, create, qw_drag *, wlr_seat_client *seat_client, wlr_data_source *source, wlr_surface *icon_surface)
    QW_FUNC_STATIC(seat, request_start_drag, void, wlr_seat *seat, wlr_drag *drag, wlr_surface *origin, uint32_t serial)
    QW_FUNC_STATIC(seat, start_drag, void, wlr_seat *seat, wlr_drag *drag, uint32_t serial)
};

QW_END_NAMESPACE
