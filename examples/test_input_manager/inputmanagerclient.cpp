// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputmanagerclient.h"

#include <QDebug>
#include <QStringList>

TreelandInputManagerV1::TreelandInputManagerV1()
    : QWaylandClientExtensionTemplate<TreelandInputManagerV1>(InterfaceVersion)
    , QtWayland::treeland_input_manager_v1()
{
}

TreelandInputManagerV1::~TreelandInputManagerV1()
{
    destroy();
}

void TreelandInputManagerV1::instantiate()
{
    initialize();
}

void TreelandInputManagerV1::treeland_input_manager_v1_capability_available(uint32_t type, struct ::wl_seat *seat)
{
    m_seat = seat;
    m_capabilities |= type;

    QStringList devices;
    if (type & device_type_mouse)
        devices << "mouse";
    if (type & device_type_touchpad)
        devices << "touchpad";
    if (type & device_type_keyboard)
        devices << "keyboard";

    qWarning() << "capability available (0x" << Qt::hex << type << Qt::dec << "):"
               << (devices.isEmpty() ? "(none)" : devices.join(", "));
}

void TreelandInputManagerV1::treeland_input_manager_v1_capability_unavailable(uint32_t type, struct ::wl_seat *seat)
{
    Q_UNUSED(seat);
    m_capabilities &= ~type;

    QStringList devices;
    if (type & device_type_mouse)
        devices << "mouse";
    if (type & device_type_touchpad)
        devices << "touchpad";
    if (type & device_type_keyboard)
        devices << "keyboard";

    qWarning() << "capability unavailable (0x" << Qt::hex << type << Qt::dec << "):"
               << (devices.isEmpty() ? "(none)" : devices.join(", "));
}

bool TreelandInputManagerV1::hasMouse() const
{
    return (m_capabilities & device_type_mouse) != 0;
}

bool TreelandInputManagerV1::hasTouchpad() const
{
    return (m_capabilities & device_type_touchpad) != 0;
}

bool TreelandInputManagerV1::hasKeyboard() const
{
    return (m_capabilities & device_type_keyboard) != 0;
}

struct ::wl_seat *TreelandInputManagerV1::seat() const
{
    return m_seat;
}

TreelandPointerDeviceConfigurationV1::TreelandPointerDeviceConfigurationV1(
    struct ::treeland_pointer_device_configuration_v1 *object,
    DeviceType type)
    : QtWayland::treeland_pointer_device_configuration_v1(object)
    , m_deviceType(type)
{
}

TreelandPointerDeviceConfigurationV1::~TreelandPointerDeviceConfigurationV1()
{
    destroy();
}

bool TreelandPointerDeviceConfigurationV1::hasFeature(uint32_t flag) const
{
    return (m_features & flag) != 0;
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_feature(uint32_t feature)
{
    m_features = feature;

    QStringList flags;
    if (feature & feature_scroll_factor)
        flags << "scroll_factor";
    if (feature & feature_handed_mode)
        flags << "handed_mode";
    if (feature & feature_accel_speed)
        flags << "accel_speed";
    if (feature & feature_acceleration_profile)
        flags << "acceleration_profile";
    if (feature & feature_send_events_mode)
        flags << "send_events_mode";
    if (feature & feature_natural_scroll)
        flags << "natural_scroll";
    if (feature & feature_disable_while_typing)
        flags << "disable_while_typing";
    if (feature & feature_tap_to_click)
        flags << "tap_to_click";

    const char *deviceName = (m_deviceType == DeviceType::Mouse) ? "mouse" : "touchpad";
    qWarning() << deviceName << "supported features (0x" << Qt::hex << feature << Qt::dec << "):" << (flags.isEmpty() ? "(none)" : flags.join(", "));
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_scroll_factor(wl_fixed_t factor)
{
    qWarning() << "scroll_factor:" << wl_fixed_to_double(factor);
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_handed_mode(uint32_t mode)
{
    if (mode == handed_mode_right)
        qWarning() << "handed_mode: right";
    else if (mode == handed_mode_left)
        qWarning() << "handed_mode: left";
    else
        qWarning() << "handed_mode: unknown(" << mode << ")";
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_accel_speed(wl_fixed_t accel_speed)
{
    qWarning() << "accel_speed:" << wl_fixed_to_double(accel_speed);
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_acceleration_profile(uint32_t profile)
{
    QStringList profiles;
    switch (profile) {
    case acceleration_profile_none:
        profiles << "none";
        break;
    case acceleration_profile_flat:
        profiles << "flat";
        break;
    case acceleration_profile_adaptive:
        profiles << "adaptive";
        break;
    case acceleration_profile_custom:
        profiles << "custom";
        break;
    default:
        break;
    }

    qWarning() << "acceleration_profile (0x" << Qt::hex << profile << Qt::dec << "):" << (profiles.isEmpty() ? "(none)" : profiles.join("|"));
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_send_events_mode(uint32_t mode)
{
    if (mode == send_events_mode_enabled)
        qWarning() << "send_events_mode: enabled";
    else if (mode == send_events_mode_disabled)
        qWarning() << "send_events_mode: disabled";
    else if (mode == send_events_mode_disabled_on_external_mouse)
        qWarning() << "send_events_mode: disabled_on_external_mouse";
    else
        qWarning() << "send_events_mode: unknown(" << mode << ")";
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_natural_scroll(uint32_t state)
{
    qWarning() << "natural_scroll:" << (state ? "enabled" : "disabled");
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_disable_while_typing(uint32_t state)
{
    qWarning() << "disable_while_typing:" << (state ? "enabled" : "disabled");
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_tap_to_click(uint32_t state)
{
    qWarning() << "tap_to_click:" << (state ? "enabled" : "disabled");
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_failed()
{
    qWarning() << "pointer device configuration: apply failed";
}

void TreelandPointerDeviceConfigurationV1::treeland_pointer_device_configuration_v1_done(uint32_t serial)
{
    const char *deviceName = (m_deviceType == DeviceType::Mouse) ? "mouse" : "touchpad";
    qWarning() << deviceName << "done, serial:" << serial;
    qWarning() << "";
}

TreelandKeyboardSettingsV1::TreelandKeyboardSettingsV1(
    struct ::treeland_keyboard_settings_v1 *object)
    : QtWayland::treeland_keyboard_settings_v1(object)
{
}

TreelandKeyboardSettingsV1::~TreelandKeyboardSettingsV1()
{
    destroy();
}

bool TreelandKeyboardSettingsV1::hasFeature(uint32_t flag) const
{
    return (m_features & flag) != 0;
}

void TreelandKeyboardSettingsV1::treeland_keyboard_settings_v1_feature(uint32_t feature)
{
    m_features = feature;

    QStringList flags;
    if (feature & feature_num_lock)
        flags << "num_lock";

    qWarning() << "keyboard supported features (0x" << Qt::hex << feature << Qt::dec << "):" << (flags.isEmpty() ? "(none)" : flags.join(", "));
}

void TreelandKeyboardSettingsV1::configureAndApply(int32_t rate, int32_t delay)
{
    set_repeat(rate, delay);
    apply();
    qWarning() << "Keyboard configuration applied, rate:" << rate
             << "delay:" << delay;
}

void TreelandKeyboardSettingsV1::treeland_keyboard_settings_v1_repeat(int32_t rate, int32_t delay)
{
    qWarning() << "repeat rate:" << rate << "delay:" << delay;
}

void TreelandKeyboardSettingsV1::treeland_keyboard_settings_v1_num_lock(uint32_t state)
{
    qWarning() << "num_lock:" << (state ? "on" : "off");
}

void TreelandKeyboardSettingsV1::treeland_keyboard_settings_v1_failed()
{
    qWarning() << "keyboard settings: apply failed";
}

void TreelandKeyboardSettingsV1::treeland_keyboard_settings_v1_done()
{
    qWarning() << "keyboard settings: done";
    qWarning() << "";
}
