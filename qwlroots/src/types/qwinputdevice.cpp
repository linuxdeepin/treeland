// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwinputdevice.h"

QW_BEGIN_NAMESPACE
qw_input_device *qw_input_device::create(HandleType *handle) {
    if (auto o = get(handle))
        return o;

    switch (handle->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        return new qw_keyboard(handle, false);
    case WLR_INPUT_DEVICE_POINTER:
        return new qw_pointer(handle, false);
    case WLR_INPUT_DEVICE_TABLET:
        return new qw_tablet(handle, false);
    case WLR_INPUT_DEVICE_TABLET_PAD:
        return new qw_tablet_pad(handle, false);
    case WLR_INPUT_DEVICE_SWITCH:
        return new qw_switch(handle, false);
    case WLR_INPUT_DEVICE_TOUCH:
        return new qw_touch(handle, false);
    default:
        // Here is not reachable
        qCritical("Unknow input device type!");
        return nullptr;
    }
}
QW_END_NAMESPACE
