// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <libinput.h>

bool configAccelSpeed(libinput_device *device, double speed);
bool configAccelProfile(libinput_device *device, libinput_config_accel_profile profile);
bool configNaturalScroll(libinput_device *device, bool natural);
bool configSendEventsMode(libinput_device *device, uint32_t mode);
bool configTapEnabled(libinput_device *device, libinput_config_tap_state tap);
bool configDwtEnabled(libinput_device *device, enum libinput_config_dwt_state enable);
bool configLeftHanded(libinput_device *device, bool left);
