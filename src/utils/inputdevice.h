// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QObject>
#include <qwglobal.h>

#include <libinput.h>

QW_BEGIN_NAMESPACE
class qw_input_device;
QW_END_NAMESPACE

QW_USE_NAMESPACE

class InputDevice : public QObject
{
public:
    static libinput_device *libinput_device_handle(qw_input_device *handle);
    static bool setSendEventsMode(qw_input_device *handle, uint32_t mode);
    static bool setTapEnabled(qw_input_device *handle, enum libinput_config_tap_state tap);
    static bool setTapButtonMap(qw_input_device *handle, enum libinput_config_tap_button_map map);
    static bool setTapDragEnabled(qw_input_device *handle, enum libinput_config_drag_state drag);
    static bool setTapDragLock(qw_input_device *handle, enum libinput_config_drag_lock_state lock);
    static bool setAccelSpeed(qw_input_device *handle, qreal speed);
    static bool setRotationAngle(qw_input_device *handle, qreal angle);
    static bool setAccelProfile(qw_input_device *handle, enum libinput_config_accel_profile profile);
    static bool setNaturalScroll(qw_input_device *handle, bool natural);
    static bool setLeftHanded(qw_input_device *handle, bool left);
    static bool setClickMethod(qw_input_device *handle, enum libinput_config_click_method method);
    static bool setMiddleEmulation(qw_input_device *handle, enum libinput_config_middle_emulation_state mid);
    static bool setScrollMethod(qw_input_device *handle, enum libinput_config_scroll_method method);
    static bool setScrollButton(qw_input_device *handle, uint32_t button);
    static bool setScrollButtonLock(qw_input_device *handle, enum libinput_config_scroll_button_lock_state lock);
    static bool setDwt(qw_input_device *handle, enum libinput_config_dwt_state enable);
    static bool setDwtp(qw_input_device *handle, enum libinput_config_dwtp_state enable);
    static bool setCalibrationMatrix(qw_input_device *handle, float mat[6]);
};
