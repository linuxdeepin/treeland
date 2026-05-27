// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-input-manager-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtension>

class QCommandLineParser;

class TreelandInputManagerV1
    : public QWaylandClientExtensionTemplate<TreelandInputManagerV1>
    , public QtWayland::treeland_input_manager_v1
{
    Q_OBJECT
public:
    static constexpr int InterfaceVersion = 1;
    explicit TreelandInputManagerV1();
    ~TreelandInputManagerV1() override;

    void instantiate();

    bool hasMouse() const;
    bool hasTouchpad() const;
    bool hasKeyboard() const;
    struct ::wl_seat *seat() const;

protected:
    void treeland_input_manager_v1_capability_available(uint32_t type, struct ::wl_seat *seat) override;
    void treeland_input_manager_v1_capability_unavailable(uint32_t type, struct ::wl_seat *seat) override;

private:
    struct ::wl_seat *m_seat = nullptr;
    uint32_t m_capabilities = 0;
};

class TreelandPointerDeviceConfigurationV1
    : public QtWayland::treeland_pointer_device_configuration_v1
{
public:
    enum class DeviceType { Mouse, Touchpad };

    explicit TreelandPointerDeviceConfigurationV1(
        struct ::treeland_pointer_device_configuration_v1 *object,
        DeviceType type);
    ~TreelandPointerDeviceConfigurationV1() override;

    bool hasFeature(uint32_t flag) const;

protected:
    void treeland_pointer_device_configuration_v1_feature(uint32_t feature) override;
    void treeland_pointer_device_configuration_v1_scroll_factor(wl_fixed_t factor) override;
    void treeland_pointer_device_configuration_v1_handed_mode(uint32_t mode) override;
    void treeland_pointer_device_configuration_v1_accel_speed(wl_fixed_t accel_speed) override;
    void treeland_pointer_device_configuration_v1_acceleration_profile(uint32_t profile) override;
    void treeland_pointer_device_configuration_v1_send_events_mode(uint32_t mode) override;
    void treeland_pointer_device_configuration_v1_natural_scroll(uint32_t state) override;
    void treeland_pointer_device_configuration_v1_disable_while_typing(uint32_t state) override;
    void treeland_pointer_device_configuration_v1_tap_to_click(uint32_t state) override;
    void treeland_pointer_device_configuration_v1_failed() override;
    void treeland_pointer_device_configuration_v1_done(uint32_t serial) override;

private:
    DeviceType m_deviceType;
    uint32_t m_features = 0;
};

class TreelandKeyboardSettingsV1
    : public QtWayland::treeland_keyboard_settings_v1
{
public:
    explicit TreelandKeyboardSettingsV1(struct ::treeland_keyboard_settings_v1 *object);
    ~TreelandKeyboardSettingsV1() override;

    bool hasFeature(uint32_t flag) const;
    void configureAndApply(int32_t rate, int32_t delay);

protected:
    void treeland_keyboard_settings_v1_feature(uint32_t feature) override;
    void treeland_keyboard_settings_v1_repeat(int32_t rate, int32_t delay) override;
    void treeland_keyboard_settings_v1_num_lock(uint32_t state) override;
    void treeland_keyboard_settings_v1_failed() override;
    void treeland_keyboard_settings_v1_done() override;

private:
    uint32_t m_features = 0;
};
