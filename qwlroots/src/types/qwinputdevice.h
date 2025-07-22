// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_switch.h>

#define static
#include <wlr/backend/drm.h>
#undef static
#include <wlr/backend/wayland.h>
#ifdef WLR_HAVE_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/backend/libinput.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(input_device)
{
    QW_OBJECT
    Q_OBJECT

public:
    static qw_input_device *create(HandleType *handle);

    QW_FUNC_MEMBER(input_device, is_wl, bool)
#ifdef WLR_HAVE_X11_BACKEND
    QW_FUNC_MEMBER(input_device, is_x11, bool)
#endif
    QW_FUNC_MEMBER(input_device, is_libinput, bool)
};

#define QW_INPUT_DEVICE(name) \
typedef qw_##name DeriveType; \
typedef wlr_##name HandleType; \
public: \
    QW_FUNC_STATIC(name, from_input_device, qw_##name*, wlr_input_device*) \
    QW_ALWAYS_INLINE wlr_##name *handle() const { \
        return reinterpret_cast<wlr_##name *>(qw_input_device::handle()); \
    } \
    QW_ALWAYS_INLINE operator wlr_##name *() const { \
        return handle(); \
    } \
    QW_ALWAYS_INLINE static qw_##name *from(wlr_input_device *handle) { \
        return qobject_cast<qw_##name *>(qw_input_device::from(handle)); \
    } \
    QW_ALWAYS_INLINE static qw_##name *from(wlr_##name *handle) { \
        return from(&handle->base); \
    } \
protected: \
using qw_input_device::qw_input_device; \
private: \
friend class qw_input_device;

class QW_EXPORT qw_keyboard : public qw_input_device
{
    Q_OBJECT
    QW_INPUT_DEVICE(keyboard)

    QW_SIGNAL(key, wlr_keyboard_key_event*)
    QW_SIGNAL(modifiers)
    QW_SIGNAL(keymap)
    QW_SIGNAL(repeat_info)

public:
    QW_FUNC_MEMBER(keyboard, get_modifiers, uint32_t)
    QW_FUNC_MEMBER(keyboard, set_keymap, bool, xkb_keymap *keymap)
    QW_FUNC_MEMBER(keyboard, set_repeat_info, void, int32_t rate_hz, int32_t delay_ms)
};

class QW_EXPORT qw_pointer : public qw_input_device
{
    Q_OBJECT
    QW_INPUT_DEVICE(pointer)

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
};

class QW_EXPORT qw_tablet : public qw_input_device
{
    Q_OBJECT
    QW_INPUT_DEVICE(tablet)

    QW_SIGNAL(axis, wlr_tablet_tool_axis_event*)
    QW_SIGNAL(proximity, wlr_tablet_tool_proximity_event*)
    QW_SIGNAL(tip, wlr_tablet_tool_tip_event*)
    QW_SIGNAL(button, wlr_tablet_tool_button_event*)
};

class QW_EXPORT qw_tablet_pad : public qw_input_device
{
    Q_OBJECT
    QW_INPUT_DEVICE(tablet_pad)

    QW_SIGNAL(button, wlr_tablet_pad_button_event*)
    QW_SIGNAL(ring, wlr_tablet_pad_ring_event*)
    QW_SIGNAL(strip, wlr_tablet_pad_strip_event*)
    QW_SIGNAL(attach_tablet, wlr_tablet_tool*)
};

class QW_EXPORT qw_switch : public qw_input_device
{
    Q_OBJECT
    QW_INPUT_DEVICE(switch)

    QW_SIGNAL(toggle, wlr_switch_toggle_event*)
};

class QW_EXPORT qw_touch : public qw_input_device
{
    Q_OBJECT
    QW_INPUT_DEVICE(touch)

    QW_SIGNAL(down, wlr_touch_down_event*)
    QW_SIGNAL(up, wlr_touch_up_event*)
    QW_SIGNAL(motion, wlr_touch_motion_event*)
    QW_SIGNAL(cancel, wlr_touch_cancel_event*)
    QW_SIGNAL(frame)
};
QW_END_NAMESPACE
