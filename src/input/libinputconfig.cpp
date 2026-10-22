// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "libinputconfig.h"
#include "common/treelandlogging.h"

#include <wlr_all.h>

#include <QLoggingCategory>

static bool ensureStatus(libinput_config_status status)
{
    if (status != LIBINPUT_CONFIG_STATUS_SUCCESS) {
        qCCritical(lcTlInput) << "Failed to apply libinput config: "
                                << libinput_config_status_to_str(status);
        return false;
    }

    return true;
}

bool configSendEventsMode(libinput_device *device, uint32_t mode)
{
    if (libinput_device_config_send_events_get_mode(device) == mode) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_send_events_set_mode " << mode;
    enum libinput_config_status status = libinput_device_config_send_events_set_mode(device, mode);

    return ensureStatus(status);
}

bool configTapEnabled(libinput_device *device, libinput_config_tap_state tap)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0) {
        qCCritical(lcTlInput) << "libinput_device_config_tap_set_enabled is invalid";

        return false;
    }

    if (libinput_device_config_tap_get_enabled(device) == tap) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_tap_set_enabled: " << tap;
    enum libinput_config_status status = libinput_device_config_tap_set_enabled(device, tap);

    return ensureStatus(status);
}

bool configTapButtonMap(libinput_device *device, libinput_config_tap_button_map map)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0) {
        qCCritical(lcTlInput) << "libinput_device_config_tap_set_button_map is invalid";

        return false;
    }

    if (libinput_device_config_tap_get_button_map(device) == map) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_tap_set_button_map: " << map;
    enum libinput_config_status status = libinput_device_config_tap_set_button_map(device, map);

    return ensureStatus(status);
}

bool configTapDragEnabled(libinput_device *device, libinput_config_drag_state drag)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0) {
        qCCritical(lcTlInput) << "libinput_device_config_tap_set_drag_enabled is invalid";

        return false;
    }

    if (libinput_device_config_tap_get_drag_enabled(device) == drag) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_tap_set_drag_enabled: " << drag;
    enum libinput_config_status status = libinput_device_config_tap_set_drag_enabled(device, drag);

    return ensureStatus(status);
}

bool configTapDragLockEnabled(libinput_device *device, libinput_config_drag_lock_state lock)
{
    if (libinput_device_config_tap_get_finger_count(device) <= 0) {
        qCCritical(lcTlInput) << "libinput_device_config_tap_set_drag_enabled is invalid";

        return false;
    }

    if (libinput_device_config_tap_get_drag_lock_enabled(device) == lock) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_tap_set_drag_lock_enabled: " << lock;
    enum libinput_config_status status =
        libinput_device_config_tap_set_drag_lock_enabled(device, lock);

    return ensureStatus(status);
}

bool configAccelSpeed(libinput_device *device, double speed)
{
    if (!libinput_device_config_accel_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_accel_set_speed is invalid";

        return false;
    }

    if (libinput_device_config_accel_get_speed(device) == speed) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_accel_set_speed: " << speed;
    enum libinput_config_status status = libinput_device_config_accel_set_speed(device, speed);

    return ensureStatus(status);
}

bool configRotationAngle(libinput_device *device, double angle)
{
    if (!libinput_device_config_rotation_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_rotation_set_angle is invalid";

        return false;
    }

    if (libinput_device_config_rotation_get_angle(device) == angle) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_rotation_set_angle: " << angle;
    enum libinput_config_status status = libinput_device_config_rotation_set_angle(device, angle);

    return ensureStatus(status);
}

bool configAccelProfile(libinput_device *device, libinput_config_accel_profile profile)
{
    if (!libinput_device_config_accel_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_accel_set_profile is invalid";

        return false;
    }

    if (libinput_device_config_accel_get_profile(device) == profile) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_accel_set_profile: " << profile;
    enum libinput_config_status status = libinput_device_config_accel_set_profile(device, profile);

    return ensureStatus(status);
}

bool configNaturalScroll(libinput_device *device, bool natural)
{
    if (!libinput_device_config_scroll_has_natural_scroll(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_scroll_set_natural_scroll_enabled is invalid";

        return false;
    }

    if (libinput_device_config_scroll_get_natural_scroll_enabled(device) == natural) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_scroll_set_natural_scroll_enabled: " << natural;
    enum libinput_config_status status =
        libinput_device_config_scroll_set_natural_scroll_enabled(device, natural);

    return ensureStatus(status);
}

bool configLeftHanded(libinput_device *device, bool left)
{
    if (!libinput_device_config_left_handed_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_left_handed_set is invalid";

        return false;
    }

    if (libinput_device_config_left_handed_get(device) == left) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_left_handed_set: " << left;
    enum libinput_config_status status = libinput_device_config_left_handed_set(device, left);

    return ensureStatus(status);
}

bool configClickMethod(libinput_device *device, libinput_config_click_method method)
{
    uint32_t click = libinput_device_config_click_get_methods(device);
    if ((click & ~LIBINPUT_CONFIG_CLICK_METHOD_NONE) == 0
        || libinput_device_config_click_get_method(device) == method) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_click_set_method: " << method;
    enum libinput_config_status status = libinput_device_config_click_set_method(device, method);

    return ensureStatus(status);
}

bool configMiddleEmulation(libinput_device *device,
                                  libinput_config_middle_emulation_state mid)
{
    if (!libinput_device_config_middle_emulation_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_middle_emulation_set_enabled is invalid";

        return false;
    }

    if (libinput_device_config_middle_emulation_get_enabled(device) == mid) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_middle_emulation_set_enabled: " << mid;
    enum libinput_config_status status =
        libinput_device_config_middle_emulation_set_enabled(device, mid);

    return ensureStatus(status);
}

bool configScrollMethod(libinput_device *device, libinput_config_scroll_method method)
{
    uint32_t scroll = libinput_device_config_scroll_get_methods(device);
    if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0
        || libinput_device_config_scroll_get_method(device) == method) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_scroll_set_method: " << method;
    enum libinput_config_status status = libinput_device_config_scroll_set_method(device, method);

    return ensureStatus(status);
}

bool configScrollButton(libinput_device *device, uint32_t button)
{
    uint32_t scroll = libinput_device_config_scroll_get_methods(device);
    if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0
        || libinput_device_config_scroll_get_button(device) == button) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_scroll_set_button: " << button;
    enum libinput_config_status status = libinput_device_config_scroll_set_button(device, button);

    return ensureStatus(status);
}

bool configScrollButtonLock(libinput_device *device,
                                   libinput_config_scroll_button_lock_state lock)
{
    uint32_t scroll = libinput_device_config_scroll_get_methods(device);
    if ((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0
        || libinput_device_config_scroll_get_button_lock(device) == lock) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_scroll_set_button: " << lock;
    enum libinput_config_status status =
        libinput_device_config_scroll_set_button_lock(device, lock);

    return ensureStatus(status);
}

bool configDwtEnabled(libinput_device *device, enum libinput_config_dwt_state enable)
{
    if (!libinput_device_config_dwt_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_dwt_set_enabled is invalid";

        return false;
    }
    if (libinput_device_config_dwt_get_enabled(device) == enable) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_dwt_set_enabled: " << enable;
    enum libinput_config_status status = libinput_device_config_dwt_set_enabled(device, enable);

    return ensureStatus(status);
}

bool configDwtpEnabled(libinput_device *device, enum libinput_config_dwtp_state enable)
{
    if (!libinput_device_config_dwtp_is_available(device)) {
        qCCritical(lcTlInput) << "libinput_device_config_dwtp_set_enabled is invalid";

        return false;
    }

    if (libinput_device_config_dwtp_get_enabled(device) == enable) {
        return false;
    }

    qCDebug(lcTlInput) << "libinput_device_config_dwt_set_enabled: " << enable;
    enum libinput_config_status status = libinput_device_config_dwtp_set_enabled(device, enable);

    return ensureStatus(status);
}

bool configCalibrationMatrix(libinput_device *device, float mat[])
{
    if (!libinput_device_config_calibration_has_matrix(device)) {
        qCCritical(lcTlInput) << "setCalibrationMatrix mat is invalid";
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
        qCDebug(lcTlInput,
                "libinput_device_config_calibration_set_matrix(%f, %f, %f, %f, "
                "%f, %f)",
                mat[0],
                mat[1],
                mat[2],
                mat[3],
                mat[4],
                mat[5]);
        status = libinput_device_config_calibration_set_matrix(device, mat);
    }
    return changed && ensureStatus(status);
}

libinput_device *libinput_device_handle(wlr_input_device *handle)
{
    return wlr_libinput_get_device_handle(handle);
}
