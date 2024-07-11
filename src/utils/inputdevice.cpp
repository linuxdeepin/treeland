// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputdevice.h"

#include <qwinputdevice.h>
#include <qwbackend.h>
#include <winputdevice.h>

#include <QDebug>
#include <QInputDevice>
#include <QPointer>
#include <QLoggingCategory>

extern "C" {
    #include <wlr/types/wlr_input_device.h>
    #include <wlr/types/wlr_keyboard.h>
}

Q_LOGGING_CATEGORY(treelandInputDevice, "treeland.inputdevice", QtWarningMsg)

static bool ensureStatus(libinput_config_status status)
{
    if (status != LIBINPUT_CONFIG_STATUS_SUCCESS) {
        qCCritical(treelandInputDevice) << "Failed to apply libinput config: "
        << libinput_config_status_to_str(status);
        return false;
    }

    return true;
}

static bool configSendEventsMode(libinput_device *device, uint32_t mode)
{
    if (libinput_device_config_send_events_get_mode(device) == mode) {
        qCCritical(treelandInputDevice) << "libinput_device_config_send_events_set_mode repeat set mode" << mode;
        return false;
    }

    qCDebug(treelandInputDevice) << "libinput_device_config_send_events_set_mode " << mode;
    enum libinput_config_status status = libinput_device_config_send_events_set_mode(device, mode);

    return ensureStatus(status);
}

static bool configTapEnabled(libinput_device *device, libinput_config_tap_state tap)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
        libinput_device_config_tap_get_enabled(device) == tap) {
        qCCritical(treelandInputDevice) << "libinput_device_config_tap_set_enabled: " << tap <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_tap_set_enabled: " << tap;
        enum libinput_config_status status = libinput_device_config_tap_set_enabled(device, tap);

        return ensureStatus(status);
}

static bool configTapButtonMap(libinput_device *device, libinput_config_tap_button_map map)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
        libinput_device_config_tap_get_button_map(device) == map) {
        qCCritical(treelandInputDevice) << "libinput_device_config_tap_set_button_map: " << map <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_tap_set_button_map: " << map;
        enum libinput_config_status status = libinput_device_config_tap_set_button_map(device, map);

        return ensureStatus(status);
}

static bool configTapDragEnabled(libinput_device *device, libinput_config_drag_state drag)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
        libinput_device_config_tap_get_drag_enabled(device) == drag) {
        qCCritical(treelandInputDevice) << "libinput_device_config_tap_set_drag_enabled: " << drag <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_tap_set_drag_enabled: " << drag;
        enum libinput_config_status status = libinput_device_config_tap_set_drag_enabled(device, drag);

        return ensureStatus(status);
}

static bool configTapDragLockEnabled(libinput_device *device, libinput_config_drag_lock_state lock)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0 ||
        libinput_device_config_tap_get_drag_lock_enabled(device) == lock) {
        qCCritical(treelandInputDevice) << "libinput_device_config_tap_set_drag_enabled: " << lock <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_tap_set_drag_lock_enabled: " << lock;
        enum libinput_config_status status = libinput_device_config_tap_set_drag_lock_enabled(device, lock);

        return ensureStatus(status);
}

static bool configAccelSpeed(libinput_device *device, double speed)
{
    if (!libinput_device_config_accel_is_available(device) ||
        libinput_device_config_accel_get_speed(device) == speed) {
        qCCritical(treelandInputDevice) << "libinput_device_config_accel_set_speed: " << speed <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_accel_set_speed: " << speed;
        enum libinput_config_status status = libinput_device_config_accel_set_speed(device, speed);

        return ensureStatus(status);
}

static bool configRotationAngle(libinput_device *device, double angle)
{
    if (!libinput_device_config_rotation_is_available(device) ||
        libinput_device_config_rotation_get_angle(device) == angle) {
        qCCritical(treelandInputDevice) << "libinput_device_config_rotation_set_angle: " << angle <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_rotation_set_angle: " << angle;
        enum libinput_config_status status = libinput_device_config_rotation_set_angle(device, angle);

        return ensureStatus(status);
}

static bool configAccelProfile(libinput_device *device, libinput_config_accel_profile profile)
{
    if (!libinput_device_config_accel_is_available(device) ||
        libinput_device_config_accel_get_profile(device) == profile) {
        qCCritical(treelandInputDevice) << "libinput_device_config_accel_set_profile: " << profile <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_accel_set_profile: " << profile;
        enum libinput_config_status status = libinput_device_config_accel_set_profile(device, profile);

        return ensureStatus(status);
}

static bool configNaturalScroll(libinput_device *device, bool natural)
{
    if (!libinput_device_config_scroll_has_natural_scroll(device) ||
        libinput_device_config_scroll_get_natural_scroll_enabled(device) == natural) {
        qCCritical(treelandInputDevice) << "libinput_device_config_scroll_set_natural_scroll_enabled: " << natural <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_scroll_set_natural_scroll_enabled: " << natural;
        enum libinput_config_status status = libinput_device_config_scroll_set_natural_scroll_enabled(device, natural);

        return ensureStatus(status);
}

static bool configLeftHanded(libinput_device *device, bool left)
{
    if (!libinput_device_config_left_handed_is_available(device) ||
        libinput_device_config_left_handed_get(device) == left) {
        qCCritical(treelandInputDevice) << "libinput_device_config_left_handed_set: " << left <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_left_handed_set: " << left;
        enum libinput_config_status status = libinput_device_config_left_handed_set(device, left);

        return ensureStatus(status);
}

static bool configClickMethod(libinput_device *device, libinput_config_click_method method)
{
    uint32_t click = libinput_device_config_click_get_methods(device);
    if ((click & ~LIBINPUT_CONFIG_CLICK_METHOD_NONE) == 0 ||
        libinput_device_config_click_get_method(device) == method) {
        qCCritical(treelandInputDevice) << "libinput_device_config_click_set_method: " << method <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_click_set_method: " << method;
        enum libinput_config_status status = libinput_device_config_click_set_method(device, method);

        return ensureStatus(status);
}

static bool configMiddleEmulation(libinput_device *device, libinput_config_middle_emulation_state mid)
{
    if (!libinput_device_config_middle_emulation_is_available(device) ||
        libinput_device_config_middle_emulation_get_enabled(device) == mid) {
        qCCritical(treelandInputDevice) << "libinput_device_config_middle_emulation_set_enabled: " << mid <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_middle_emulation_set_enabled: " << mid;
        enum libinput_config_status status = libinput_device_config_middle_emulation_set_enabled(device, mid);

        return ensureStatus(status);
}

static bool configScrollMethod(libinput_device *device, libinput_config_scroll_method method)
{
    uint32_t scroll = libinput_device_config_scroll_get_methods(device);
    if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
        libinput_device_config_scroll_get_method(device) == method) {
        qCCritical(treelandInputDevice) << "libinput_device_config_scroll_set_method: " << method <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_scroll_set_method: " << method;
        enum libinput_config_status status = libinput_device_config_scroll_set_method(device, method);

        return ensureStatus(status);
}

static bool configScrollButton(libinput_device *device, uint32_t button)
{
    uint32_t scroll = libinput_device_config_scroll_get_methods(device);
    if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
        libinput_device_config_scroll_get_button(device) == button) {
        qCCritical(treelandInputDevice) << "libinput_device_config_scroll_set_button: " << button <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_scroll_set_button: " << button;
        enum libinput_config_status status = libinput_device_config_scroll_set_button(device, button);

        return ensureStatus(status);
}

static bool configScrollButtonLock(libinput_device *device, libinput_config_scroll_button_lock_state lock)
{
    uint32_t scroll = libinput_device_config_scroll_get_methods(device);
    if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
        libinput_device_config_scroll_get_button_lock(device) == lock) {
        qCCritical(treelandInputDevice) << "libinput_device_config_scroll_set_button_lock: " << lock <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_scroll_set_button: " << lock;
        enum libinput_config_status status = libinput_device_config_scroll_set_button_lock(device, lock);

        return ensureStatus(status);
}

static bool configDwtEnabled(libinput_device *device, enum libinput_config_dwt_state enable)
{
    if (!libinput_device_config_dwt_is_available(device) ||
        libinput_device_config_dwt_get_enabled(device) == enable) {
        qCCritical(treelandInputDevice) << "libinput_device_config_dwt_set_enabled: " << enable <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_dwt_set_enabled: " << enable;
        enum libinput_config_status status = libinput_device_config_dwt_set_enabled(device, enable);

        return ensureStatus(status);
}

static bool configDwtpEnabled(libinput_device *device, enum libinput_config_dwtp_state enable)
{
    if (!libinput_device_config_dwtp_is_available(device) ||
        libinput_device_config_dwtp_get_enabled(device) == enable) {
        qCCritical(treelandInputDevice) << "libinput_device_config_dwtp_set_enabled: " << enable <<" is invalid";
    return false;
        }

        qCDebug(treelandInputDevice) << "libinput_device_config_dwt_set_enabled: " << enable;
        enum libinput_config_status status = libinput_device_config_dwtp_set_enabled(device, enable);

        return ensureStatus(status);
}

static bool configCalibrationMatrix(libinput_device *device, float mat[])
{
    if (!libinput_device_config_calibration_has_matrix(device)) {
        qCCritical(treelandInputDevice) << "setCalibrationMatrix mat is invalid";
        return false;
    }
    bool changed = false;
    float current[6];
    libinput_device_config_calibration_get_matrix(device, current);
    enum libinput_config_status status = LIBINPUT_CONFIG_STATUS_UNSUPPORTED;
    for (int i = 0; i < 6; i++) {
        if (current[i] != mat[i]) {
            changed = true;
            break;
        }
    }
    if (changed) {
        qCDebug(treelandInputDevice, "libinput_device_config_calibration_set_matrix(%f, %f, %f, %f, %f, %f)",
                mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
        status = libinput_device_config_calibration_set_matrix(device, mat);
    }
    return changed && ensureStatus(status);
}

libinput_device *InputDevice::libinput_device_handle(QWInputDevice *handle)
{
    return QWLibinputBackend::getDeviceHandle(handle);
}

bool InputDevice::setSendEventsMode(QWInputDevice *handle, uint32_t mode)
{
    return configSendEventsMode(libinput_device_handle(handle), mode);
}

bool InputDevice::setTapButtonMap(QWInputDevice *handle, libinput_config_tap_button_map map)
{
    return configTapButtonMap(libinput_device_handle(handle), map);
}

bool InputDevice::setTapEnabled(QWInputDevice *handle, libinput_config_tap_state tap)
{
    return configTapEnabled(libinput_device_handle(handle), tap);
}

bool InputDevice::setTapDragEnabled(QWInputDevice *handle, libinput_config_drag_state drag)
{
    return configTapDragEnabled(libinput_device_handle(handle), drag);
}

bool InputDevice::setTapDragLock(QWInputDevice *handle, libinput_config_drag_lock_state lock)
{
    return configTapDragLockEnabled(libinput_device_handle(handle), lock);
}

bool InputDevice::setAccelSpeed(QWInputDevice *handle, qreal speed)
{
    return configAccelSpeed(libinput_device_handle(handle), speed);
}

bool InputDevice::setRotationAngle(QWInputDevice *handle, qreal angle)
{
    return configRotationAngle(libinput_device_handle(handle), angle);
}

bool InputDevice::setAccelProfile(QWInputDevice *handle, libinput_config_accel_profile profile)
{
    return configAccelProfile(libinput_device_handle(handle), profile);
}

bool InputDevice::setNaturalScroll(QWInputDevice *handle, bool natural)
{
    return configNaturalScroll(libinput_device_handle(handle), natural);
}

bool InputDevice::setLeftHanded(QWInputDevice *handle, bool left)
{
    return configLeftHanded(libinput_device_handle(handle), left);
}

bool InputDevice::setClickMethod(QWInputDevice *handle, libinput_config_click_method method)
{
    return configClickMethod(libinput_device_handle(handle), method);
}

bool InputDevice::setMiddleEmulation(QWInputDevice *handle, libinput_config_middle_emulation_state mid)
{
    return configMiddleEmulation(libinput_device_handle(handle), mid);
}

bool InputDevice::setScrollMethod(QWInputDevice *handle, libinput_config_scroll_method method)
{
    return configScrollMethod(libinput_device_handle(handle), method);
}

bool InputDevice::setScrollButton(QWInputDevice *handle, uint32_t button)
{
    return configScrollButton(libinput_device_handle(handle), button);
}

bool InputDevice::setScrollButtonLock(QWInputDevice *handle, libinput_config_scroll_button_lock_state lock)
{
    return configScrollButtonLock(libinput_device_handle(handle), lock);
}

bool InputDevice::setDwt(QWInputDevice *handle, libinput_config_dwt_state enable)
{
    return configDwtEnabled(libinput_device_handle(handle), enable);
}

bool InputDevice::setDwtp(QWInputDevice *handle, libinput_config_dwtp_state enable)
{
    return configDwtpEnabled(libinput_device_handle(handle), enable);
}

bool InputDevice::setCalibrationMatrix(QWInputDevice *handle, float mat[])
{
    return configCalibrationMatrix(libinput_device_handle(handle), mat);
}
