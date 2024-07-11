// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QObject>
#include <qwglobal.h>

#include <libinput.h>

QW_BEGIN_NAMESPACE
class QWInputDevice;
QW_END_NAMESPACE

QW_USE_NAMESPACE

class InputDevice : public QObject
{
public:
    static libinput_device *libinput_device_handle(QWInputDevice *handle);
    static bool setSendEventsMode(QWInputDevice *handle, uint32_t mode);
    static bool setTapEnabled(QWInputDevice *handle, enum libinput_config_tap_state tap);
    static bool setTapButtonMap(QWInputDevice *handle, enum libinput_config_tap_button_map map);
    static bool setTapDragEnabled(QWInputDevice *handle, enum libinput_config_drag_state drag);
    static bool setTapDragLock(QWInputDevice *handle, enum libinput_config_drag_lock_state lock);
    static bool setAccelSpeed(QWInputDevice *handle, qreal speed);
    static bool setRotationAngle(QWInputDevice *handle, qreal angle);
    static bool setAccelProfile(QWInputDevice *handle, enum libinput_config_accel_profile profile);
    static bool setNaturalScroll(QWInputDevice *handle, bool natural);
    static bool setLeftHanded(QWInputDevice *handle, bool left);
    static bool setClickMethod(QWInputDevice *handle, enum libinput_config_click_method method);
    static bool setMiddleEmulation(QWInputDevice *handle, enum libinput_config_middle_emulation_state mid);
    static bool setScrollMethod(QWInputDevice *handle, enum libinput_config_scroll_method method);
    static bool setScrollButton(QWInputDevice *handle, uint32_t button);
    static bool setScrollButtonLock(QWInputDevice *handle, enum libinput_config_scroll_button_lock_state lock);
    static bool setDwt(QWInputDevice *handle, enum libinput_config_dwt_state enable);
    static bool setDwtp(QWInputDevice *handle, enum libinput_config_dwtp_state enable);
    static bool setCalibrationMatrix(QWInputDevice *handle, float mat[6]);
};
