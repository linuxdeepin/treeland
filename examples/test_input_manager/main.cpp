// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputmanagerclient.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDebug>

#include <QtGui/private/qguiapplication_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>

using Manager = QtWayland::treeland_input_manager_v1;
using PointerConfig = QtWayland::treeland_pointer_device_configuration_v1;

static void  roundtrip() {
    auto *integration =
        static_cast<QtWaylandClient::QWaylandIntegration *>(
            QGuiApplicationPrivate::platformIntegration());
    wl_display_dispatch(integration->display()->wl_display());
    wl_display_roundtrip(integration->display()->wl_display());
}

static void configureMouseSettings(QtWayland::treeland_mouse_settings_v1 *mouseSettings,
                                   const QCommandLineParser &parser)
{
    auto *rawConfig = mouseSettings->get_pointer_configuration(0);
    if (!rawConfig) {
        qWarning() << "Failed to get pointer configuration for mouse";
        return;
    }

    auto *config = new TreelandPointerDeviceConfigurationV1(rawConfig, TreelandPointerDeviceConfigurationV1::DeviceType::Mouse);
    roundtrip();
    bool anySet = false;

    if (parser.isSet("mouse-scroll-factor") && config->hasFeature(PointerConfig::feature_scroll_factor)) {
        bool ok = false;
        double factor = parser.value("mouse-scroll-factor").toDouble(&ok);
        if (ok) {
            config->set_scroll_factor(wl_fixed_from_double(factor));
            anySet = true;
        }
    }
    if (parser.isSet("mouse-accel-speed") && config->hasFeature(PointerConfig::feature_accel_speed)) {
        bool ok = false;
        double speed = parser.value("mouse-accel-speed").toDouble(&ok);
        if (ok) {
            config->set_accel_speed(wl_fixed_from_double(speed));
            anySet = true;
        }
    }
    if (parser.isSet("mouse-accel-profile") && config->hasFeature(PointerConfig::feature_acceleration_profile)) {
        auto profileStr = parser.value("mouse-accel-profile");
        uint32_t profile = PointerConfig::acceleration_profile_adaptive;
        if (profileStr == "none")
            profile = PointerConfig::acceleration_profile_none;
        else if (profileStr == "flat")
            profile = PointerConfig::acceleration_profile_flat;
        else if (profileStr == "custom")
            profile = PointerConfig::acceleration_profile_custom;
        config->set_acceleration_profile(profile);
        anySet = true;
    }
    if (parser.isSet("mouse-handed-mode") && config->hasFeature(PointerConfig::feature_handed_mode)) {
        auto modeStr = parser.value("mouse-handed-mode");
        uint32_t mode = TreelandPointerDeviceConfigurationV1::handed_mode::handed_mode_right;
        if (modeStr == "left")
            mode = TreelandPointerDeviceConfigurationV1::handed_mode::handed_mode_left;
        config->set_handed_mode(mode);
        anySet = true;
    }
    if (parser.isSet("mouse-natural-scroll") && config->hasFeature(PointerConfig::feature_natural_scroll)) {
        uint32_t state = parser.value("mouse-natural-scroll") == "disabled"
            ? Manager::state_disabled : Manager::state_enabled;
        config->set_natural_scroll(state);
        anySet = true;
    }

    if (anySet) {
        config->apply();
        qDebug() << "Mouse configuration applied";
    }
}

static void configureTouchpadSettings(QtWayland::treeland_touchpad_settings_v1 *touchpadSettings,
                                      const QCommandLineParser &parser)
{
    auto *rawConfig = touchpadSettings->get_pointer_configuration(0);
    if (!rawConfig) {
        qWarning() << "Failed to get pointer configuration for touchpad";
        return;
    }

    auto *config = new TreelandPointerDeviceConfigurationV1(rawConfig, TreelandPointerDeviceConfigurationV1::DeviceType::Touchpad);
    roundtrip();
    bool anySet = false;

    if (parser.isSet("touchpad-accel-speed") && config->hasFeature(PointerConfig::feature_accel_speed)) {
        bool ok = false;
        double speed = parser.value("touchpad-accel-speed").toDouble(&ok);
        if (ok) {
            config->set_accel_speed(wl_fixed_from_double(speed));
            anySet = true;
        }
    }
    if (parser.isSet("touchpad-accel-profile") && config->hasFeature(PointerConfig::feature_acceleration_profile)) {
        auto profileStr = parser.value("touchpad-accel-profile");
        uint32_t profile = PointerConfig::acceleration_profile_adaptive;
        if (profileStr == "none")
            profile = PointerConfig::acceleration_profile_none;
        else if (profileStr == "flat")
            profile = PointerConfig::acceleration_profile_flat;
        else if (profileStr == "custom")
            profile = PointerConfig::acceleration_profile_custom;
        config->set_acceleration_profile(profile);
        anySet = true;
    }
    if (parser.isSet("touchpad-send-events-mode") && config->hasFeature(PointerConfig::feature_send_events_mode)) {
        auto modeStr = parser.value("touchpad-send-events-mode");
        uint32_t mode = PointerConfig::send_events_mode_enabled;
        if (modeStr == "disabled")
            mode = PointerConfig::send_events_mode_disabled;
        else if (modeStr == "disabled-on-external-mouse")
            mode = PointerConfig::send_events_mode_disabled_on_external_mouse;
        config->set_send_events_mode(mode);
        anySet = true;
    }
    if (parser.isSet("touchpad-natural-scroll") && config->hasFeature(PointerConfig::feature_natural_scroll)) {
        uint32_t state = parser.value("touchpad-natural-scroll") == "disabled"
            ? Manager::state_disabled : Manager::state_enabled;
        config->set_natural_scroll(state);
        anySet = true;
    }
    if (parser.isSet("touchpad-disable-while-typing") && config->hasFeature(PointerConfig::feature_disable_while_typing)) {
        uint32_t state = parser.value("touchpad-disable-while-typing") == "disabled"
            ? Manager::state_disabled : Manager::state_enabled;
        config->set_disable_while_typing(state);
        anySet = true;
    }
    if (parser.isSet("touchpad-tap-to-click") && config->hasFeature(PointerConfig::feature_tap_to_click)) {
        uint32_t state = parser.value("touchpad-tap-to-click") == "disabled"
            ? Manager::state_disabled : Manager::state_enabled;
        config->set_tap_to_click(state);
        anySet = true;
    }

    if (anySet) {
        config->apply();
        qDebug() << "Touchpad configuration applied";
    }
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "wayland");

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Treeland Input Manager Client"));
    parser.addHelpOption();

    // Mouse options
    QCommandLineOption mouseScrollFactorOption(
        QStringLiteral("mouse-scroll-factor"),
        QStringLiteral("Mouse scroll factor (e.g. 1.0)."),
        QStringLiteral("factor")
    );
    parser.addOption(mouseScrollFactorOption);

    QCommandLineOption mouseAccelSpeedOption(
        QStringLiteral("mouse-accel-speed"),
        QStringLiteral("Mouse acceleration speed (e.g. 0.0)."),
        QStringLiteral("speed")
    );
    parser.addOption(mouseAccelSpeedOption);

    QCommandLineOption mouseAccelProfileOption(
        QStringLiteral("mouse-accel-profile"),
        QStringLiteral("Mouse acceleration profile: none | flat | adaptive | custom."),
        QStringLiteral("profile")
    );
    parser.addOption(mouseAccelProfileOption);

    QCommandLineOption mouseNaturalScrollOption(
        QStringLiteral("mouse-natural-scroll"),
        QStringLiteral("Mouse natural scrolling: enabled | disabled."),
        QStringLiteral("state")
    );
    parser.addOption(mouseNaturalScrollOption);

    QCommandLineOption mouseHandedModeOption(
        QStringLiteral("mouse-handed-mode"),
        QStringLiteral("Mouse handed mode: left | right."),
        QStringLiteral("mode")
    );
    parser.addOption(mouseHandedModeOption);

    // Touchpad options
    QCommandLineOption touchpadAccelSpeedOption(
        QStringLiteral("touchpad-accel-speed"),
        QStringLiteral("Touchpad acceleration speed (e.g. 0.0)."),
        QStringLiteral("speed")
    );
    parser.addOption(touchpadAccelSpeedOption);

    QCommandLineOption touchpadAccelProfileOption(
        QStringLiteral("touchpad-accel-profile"),
        QStringLiteral("Touchpad acceleration profile: none | flat | adaptive | custom."),
        QStringLiteral("profile")
    );
    parser.addOption(touchpadAccelProfileOption);

    QCommandLineOption touchpadNaturalScrollOption(
        QStringLiteral("touchpad-natural-scroll"),
        QStringLiteral("Touchpad natural scrolling: enabled | disabled."),
        QStringLiteral("state")
    );
    parser.addOption(touchpadNaturalScrollOption);

    QCommandLineOption touchpadDisableWhileTypingOption(
        QStringLiteral("touchpad-disable-while-typing"),
        QStringLiteral("Touchpad disable-while-typing: enabled | disabled."),
        QStringLiteral("state")
    );
    parser.addOption(touchpadDisableWhileTypingOption);

    QCommandLineOption touchpadTapToClickOption(
        QStringLiteral("touchpad-tap-to-click"),
        QStringLiteral("Touchpad tap-to-click: enabled | disabled."),
        QStringLiteral("state")
    );
    parser.addOption(touchpadTapToClickOption);

    QCommandLineOption touchpadSendEventsModeOption(
        QStringLiteral("touchpad-send-events-mode"),
        QStringLiteral("Touchpad send events mode: enabled | disabled | disabled-on-external-mouse."),
        QStringLiteral("mode")
    );
    parser.addOption(touchpadSendEventsModeOption);

    // Keyboard options
    QCommandLineOption keyboardRepeatRateOption(
        QStringLiteral("keyboard-repeat-rate"),
        QStringLiteral("Keyboard repeat rate in characters per second (e.g. 25)."),
        QStringLiteral("rate")
    );
    parser.addOption(keyboardRepeatRateOption);

    QCommandLineOption keyboardRepeatDelayOption(
        QStringLiteral("keyboard-repeat-delay"),
        QStringLiteral("Keyboard repeat delay in milliseconds (e.g. 300)."),
        QStringLiteral("delay")
    );
    parser.addOption(keyboardRepeatDelayOption);

    QCommandLineOption keyboardNumLockOption(
        QStringLiteral("keyboard-num-lock"),
        QStringLiteral("Keyboard num lock state: enabled | disabled."),
        QStringLiteral("state")
    );
    parser.addOption(keyboardNumLockOption);

    parser.process(app);

    auto *manager = new TreelandInputManagerV1;

    manager->instantiate();
    roundtrip();

    if (!manager->isActive()) {
        qCritical() << "treeland_input_manager_v1 not available";
        return EXIT_FAILURE;
    }

    auto *seat = manager->seat();
    if (!seat) {
        qCritical() << "No seat received from capability events";
        return EXIT_FAILURE;
    }

    qWarning() << "Available capabilities:"
               << "mouse:" << manager->hasMouse()
               << "touchpad:" << manager->hasTouchpad()
               << "keyboard:" << manager->hasKeyboard();

    if (manager->hasMouse()) {
        auto *mouseObj = manager->get_mouse_settings(seat);
        if (mouseObj) {
            auto *mouseSettings = new QtWayland::treeland_mouse_settings_v1(mouseObj);
            configureMouseSettings(mouseSettings, parser);
        } else {
            qWarning() << "Failed to get mouse settings";
        }
    } else {
        qWarning() << "No mouse capability, skipping mouse settings";
    }

    if (manager->hasTouchpad()) {
        auto *touchpadObj = manager->get_touchpad_settings(seat);
        if (touchpadObj) {
            auto *touchpadSettings = new QtWayland::treeland_touchpad_settings_v1(touchpadObj);
            configureTouchpadSettings(touchpadSettings, parser);
        } else {
            qWarning() << "Failed to get touchpad settings";
        }
    } else {
        qWarning() << "No touchpad capability, skipping touchpad settings";
    }

    if (manager->hasKeyboard()) {
        auto *keyboardObj = manager->get_keyboard_settings(seat);
        if (keyboardObj) {
            auto *keyboardSettings = new TreelandKeyboardSettingsV1(keyboardObj);
            roundtrip();

            if (parser.isSet("keyboard-repeat-rate") || parser.isSet("keyboard-repeat-delay")) {
                int32_t rate = parser.isSet("keyboard-repeat-rate")
                    ? parser.value("keyboard-repeat-rate").toInt() : 25;
                int32_t delay = parser.isSet("keyboard-repeat-delay")
                    ? parser.value("keyboard-repeat-delay").toInt() : 300;

                keyboardSettings->configureAndApply(rate, delay);
            }

            if (parser.isSet("keyboard-num-lock") && keyboardSettings->hasFeature(QtWayland::treeland_keyboard_settings_v1::feature_num_lock)) {
                uint32_t state = parser.value("keyboard-num-lock") == "disabled"
                    ? QtWayland::treeland_keyboard_settings_v1::toggle_state_off
                    : QtWayland::treeland_keyboard_settings_v1::toggle_state_on;
                keyboardSettings->set_num_lock(state);
                keyboardSettings->apply();
                qDebug() << "Keyboard num lock applied:" << (state ? "on" : "off");
            }
        } else {
            qWarning() << "Failed to get keyboard settings";
        }
    } else {
        qWarning() << "No keyboard capability, skipping keyboard settings";
    }

    return app.exec();
}
