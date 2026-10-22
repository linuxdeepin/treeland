// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <libinput.h>

struct wlr_input_device;

bool configSendEventsMode(libinput_device *device, uint32_t mode);
bool configTapEnabled(libinput_device *device, libinput_config_tap_state tap);
bool configTapButtonMap(libinput_device *device, libinput_config_tap_button_map map);
bool configTapDragEnabled(libinput_device *device, libinput_config_drag_state drag);
bool configTapDragLockEnabled(libinput_device *device, libinput_config_drag_lock_state lock);
bool configAccelSpeed(libinput_device *device, double speed);
bool configRotationAngle(libinput_device *device, double angle);
bool configAccelProfile(libinput_device *device, libinput_config_accel_profile profile);
bool configNaturalScroll(libinput_device *device, bool natural);
bool configLeftHanded(libinput_device *device, bool left);
bool configClickMethod(libinput_device *device, libinput_config_click_method method);
bool configMiddleEmulation(libinput_device *device, libinput_config_middle_emulation_state mid);
bool configScrollMethod(libinput_device *device, libinput_config_scroll_method method);
bool configScrollButton(libinput_device *device, uint32_t button);
bool configScrollButtonLock(libinput_device *device, libinput_config_scroll_button_lock_state lock);
bool configDwtEnabled(libinput_device *device, enum libinput_config_dwt_state enable);
bool configDwtpEnabled(libinput_device *device, enum libinput_config_dwtp_state enable);
bool configCalibrationMatrix(libinput_device *device, float mat[]);

libinput_device *libinput_device_handle(wlr_input_device *handle);
