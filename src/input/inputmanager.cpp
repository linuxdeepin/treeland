// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <utility>
#include "inputmanager.h"
#include "dconfig/dconfigmanager.h"
#include "seatuserconfig.hpp"
#include "libinputconfig.h"
#include "gesturemanager.h"
#include "common/treelandlogging.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "seat/seatmanager.h"
#include "xsettings/settingmanager.h"

#include <libinput.h>
#include <xkbcommon/xkbcommon.h>

#include <wlr_all.h>

#include <wcursor.h>
#include <winputdevice.h>
#include <wseat.h>
#include <wbackend.h>

InputManager::InputManager(ConfigProvider configuration, SeatProvider seats, SeatManager *hardware,
                           WBackend *backend, QObject *parent)
    : QObject(parent)
    , m_configuration(std::move(configuration))
    , m_seats(std::move(seats))
    , m_hardware(hardware)
    , m_backend(backend)
{
}

InputManager::~InputManager()
{
}

void InputManager::activate()
{
    m_attached = true;
    auto *seats = m_hardware;
    connect(seats, &SeatManager::seatAdded, this, &InputManager::setupSeat, Qt::UniqueConnection);
    connect(seats, &SeatManager::seatRemoved, this, &InputManager::onSeatRemoved, Qt::UniqueConnection);
    connect(seats, &SeatManager::deviceAssigned, this, &InputManager::onInputAssigned, Qt::UniqueConnection);
    connect(this, &InputManager::settingsChanged, this, &InputManager::setupSeat, Qt::UniqueConnection);
    refresh();
}

void InputManager::deactivate()
{
    m_attached = false;
    disconnect(m_hardware, nullptr, this, nullptr);
    disconnect(this, &InputManager::settingsChanged, this, &InputManager::setupSeat);
}

void InputManager::refresh()
{
    for (auto *seat : m_seats())
        setupSeat(seat);
}

SeatUserDConfig *InputManager::seatUserConfig(WSeat *seat) const
{
    auto *config = m_configuration(seat->name());
    Q_ASSERT(config);

    return config;
}

void InputManager::setupSeat(WSeat *seat)
{
    auto *config = seatUserConfig(seat);
    connect(config, &SeatUserDConfig::valueChanged,
            this, &InputManager::refresh, Qt::UniqueConnection);
    if (!configurationReady(config)) {
        connect(config, &SeatUserDConfig::configInitializeSucceed,
                this, &InputManager::refresh, Qt::UniqueConnection);
        connect(config, &SeatUserDConfig::configInitializeFailed,
                this, &InputManager::refresh, Qt::UniqueConnection);
        return;
    }
    if (auto *cursor = seat->cursor())
        cursor->setScrollFactor(config->pointerScrollFactor());

    if (m_gestures)
        m_gestures->setHoldTimeout(config->touchpadHoldTimeoutMs());

    applyXkbConfigForSeat(seat);

    const auto devices = seat->deviceList();
    for (WInputDevice *device : devices)
        onInputAssigned(device);

    applyNumLockToKeyboards();
}

void InputManager::onMouseSettingsCreated(MouseSettingsInterfaceV1 *interface)
{
    connect(interface,
            &MouseSettingsInterfaceV1::pointerDeviceConfigurationCreated,
            this,
            &InputManager::onMousePointerConfigCreated);
}

void InputManager::onTouchpadSettingsCreated(TouchpadSettingsInterfaceV1 *interface)
{
    connect(interface,
            &TouchpadSettingsInterfaceV1::pointerDeviceConfigurationCreated,
            this,
            &InputManager::onTouchpadPointerConfigCreated);
}

void InputManager::onKeyboardSettingsCreated(KeyboardSettingsInterfaceV1 *interface)
{
    initializeKeyboardSettings(interface);
}

bool InputManager::initializeKeyboardSettings(KeyboardSettingsInterfaceV1 *interface)
{
    if (interface->property("_treelandKeyboardSettingsInitialized").toBool())
        return true;

    auto *seatConfig = seatUserConfig(interface->wSeat());

    if (!configurationReady(seatConfig)) {
        const auto initialize = [this, interface = QPointer<KeyboardSettingsInterfaceV1>(interface)] {
            if (interface)
                initializeKeyboardSettings(interface);
        };
        connect(seatConfig, &SeatUserDConfig::configInitializeSucceed, this, initialize);
        connect(seatConfig, &SeatUserDConfig::configInitializeFailed, this, initialize);
        return false;
    }

    KeyboardSettingsInterfaceV1::FeatureFlags features;
    const auto inputDevices = m_backend->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (!wlr_input_device_is_libinput(device->handle())) {
            continue;
        }

        if (device->type() != WInputDevice::Type::Keyboard)
            continue;

        if (device->handle()->type != WLR_INPUT_DEVICE_KEYBOARD)
            continue;

        auto *wlrKeyboard = wlr_keyboard_from_input_device(device->handle());
        if (!wlrKeyboard || !wlrKeyboard->keymap)
            continue;

        if (xkb_map_mod_get_index(wlrKeyboard->keymap, XKB_MOD_NAME_NUM) != XKB_MOD_INVALID) {
            features.setFlag(KeyboardSettingsInterfaceV1::NumLock);
            break;
        }
    }

    interface->sendFeature(features, true);
    interface->sendNumLock(seatConfig->keyboardNumLock(), true);
    interface->sendRepeat(seatConfig->keyboardRate(), seatConfig->keyboardDelay(), true);
    interface->sendXkbRules(seatConfig->xkbLayout(),
                            seatConfig->xkbModel(),
                            seatConfig->xkbVariant(),
                            seatConfig->xkbOptions(),
                            true);
    interface->sendDone();

    connect(interface,
            &KeyboardSettingsInterfaceV1::applied,
            this,
            &InputManager::onKeyboardSettingsApplied);

    interface->setProperty("_treelandKeyboardSettingsInitialized", true);

    return true;
}

void InputManager::onMousePointerConfigCreated(PointerDeviceConfigurationV1 *config)
{
    auto *ownerConfig = seatUserConfig(config->wSeat());
    if (!configurationReady(ownerConfig)) {
        const auto initialize = [this, interface = QPointer<PointerDeviceConfigurationV1>(config)] {
            if (interface)
                onMousePointerConfigCreated(interface);
        };
        connect(ownerConfig, &SeatUserDConfig::configInitializeSucceed, this, initialize);
        connect(ownerConfig, &SeatUserDConfig::configInitializeFailed, this, initialize);
        return;
    }
    auto *seatConfig = seatUserConfig(config->wSeat());

    PointerDeviceConfigurationV1::FeatureFlags features;
    features.setFlag(PointerDeviceConfigurationV1::ScrollFactor);
    features.setFlag(PointerDeviceConfigurationV1::EventsMode);

    const auto inputDevices = m_backend->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (!wlr_input_device_is_libinput(device->handle())) {
            continue;
        }

        if (device->type() != WInputDevice::Type::Pointer) {
            continue;
        }

        struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle());
        struct udev_device *udevDevice =
            libinput_device_get_udev_device(inputDevice);
        if (!udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
            continue;
        }

        if (libinput_device_config_scroll_has_natural_scroll(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::NaturalScroll);
        }

        if (libinput_device_config_left_handed_is_available(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::HandMode);
        }

        if (libinput_device_config_accel_is_available(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::AccelProfile);
            features.setFlag(PointerDeviceConfigurationV1::AccelSpeed);
        }

        if (libinput_device_config_tap_get_finger_count(inputDevice) > 0) {
            features.setFlag(PointerDeviceConfigurationV1::TapToClick);
        }

        if (libinput_device_config_dwt_is_available(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::DisableWhileTyping);
        }
    }

    config->sendFeature(features, true);
    config->sendScrollFactor(seatConfig->pointerScrollFactor(), true);

    auto handModeStr = seatConfig->pointerHandMode();
    auto handMode = (handModeStr == "Left")
        ? PointerDeviceConfigurationV1::Left
        : PointerDeviceConfigurationV1::Right;
    config->sendHandedMode(handMode, true);

    config->sendAccelSpeed(seatConfig->mouseAccelSpeed(), true);
    config->sendAccelerationProfile(static_cast<PointerDeviceConfigurationV1::AccelerationProfile>(seatConfig->mouseAccelerationProfile()), true);
    config->sendNaturalScroll(seatConfig->mouseNaturalScroll(), true);
    config->sendDone(0);

    connect(config,
            &PointerDeviceConfigurationV1::applied,
            this,
            &InputManager::onMousePointerConfigApplied);
}

void InputManager::onMousePointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes)
{
    auto *interface = static_cast<PointerDeviceConfigurationV1 *>(sender());
    auto *seatConfig = seatUserConfig(interface->wSeat());

    if (changes.testFlag(PointerDeviceConfigurationV1::ScrollFactorChanged)) {
        seatConfig->setPointerScrollFactor(interface->scrollFactor());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::HandedModeChanged)) {
        seatConfig->setPointerHandMode(interface->handedMode() == PointerDeviceConfigurationV1::Left
                                              ? QStringLiteral("Left")
                                              : QStringLiteral("Right"));
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)) {
        seatConfig->setMouseAccelSpeed(interface->accelSpeed());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged)) {
        seatConfig->setMouseAccelerationProfile(interface->accelerationProfile());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)) {
        seatConfig->setMouseNaturalScroll(interface->naturalScroll());
    }

    Q_EMIT settingsChanged(interface->wSeat());
}

void InputManager::onTouchpadPointerConfigCreated(PointerDeviceConfigurationV1 *config)
{
    auto *ownerConfig = seatUserConfig(config->wSeat());
    if (!configurationReady(ownerConfig)) {
        const auto initialize = [this, interface = QPointer<PointerDeviceConfigurationV1>(config)] {
            if (interface)
                onTouchpadPointerConfigCreated(interface);
        };
        connect(ownerConfig, &SeatUserDConfig::configInitializeSucceed, this, initialize);
        connect(ownerConfig, &SeatUserDConfig::configInitializeFailed, this, initialize);
        return;
    }
    PointerDeviceConfigurationV1::FeatureFlags features;
    features.setFlag(PointerDeviceConfigurationV1::ScrollFactor);
    features.setFlag(PointerDeviceConfigurationV1::EventsMode);

    const auto inputDevices = m_backend->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (!wlr_input_device_is_libinput(device->handle())) {
            continue;
        }

        if (device->type() != WInputDevice::Type::Pointer) {
            continue;
        }

        struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle());
        struct udev_device *udevDevice =
            libinput_device_get_udev_device(inputDevice);
        if (!udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
            continue;
        }

        if (libinput_device_config_scroll_has_natural_scroll(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::NaturalScroll);
        }

        if (libinput_device_config_left_handed_is_available(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::HandMode);
        }

        if (libinput_device_config_accel_is_available(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::AccelProfile);
            features.setFlag(PointerDeviceConfigurationV1::AccelSpeed);
        }

        if (libinput_device_config_tap_get_finger_count(inputDevice) > 0) {
            features.setFlag(PointerDeviceConfigurationV1::TapToClick);
        }

        if (libinput_device_config_dwt_is_available(inputDevice)) {
            features.setFlag(PointerDeviceConfigurationV1::DisableWhileTyping);
        }
    }

    auto *seatConfig = seatUserConfig(config->wSeat());

    config->sendFeature(features, true);
    config->sendScrollFactor(seatConfig->pointerScrollFactor(), true);
    config->sendAccelSpeed(seatConfig->touchpadAccelSpeed(), true);
    config->sendAccelerationProfile(static_cast<PointerDeviceConfigurationV1::AccelerationProfile>(seatConfig->touchpadAccelerationProfile()), true);
    config->sendNaturalScroll(seatConfig->touchpadNaturalScroll(), true);
    config->sendSendEventsMode(PointerDeviceConfigurationV1::SendEventsModes::fromInt(seatConfig->touchpadSendEventsMode()), true);
    config->sendDisableWhileTyping(seatConfig->touchpadDisableWhileTyping(), true);
    config->sendTapToClick(seatConfig->touchpadTapToClick(), true);
    config->sendDone(0);

    connect(config,
            &PointerDeviceConfigurationV1::applied,
            this,
            &InputManager::onTouchpadPointerConfigApplied);
}

void InputManager::onTouchpadPointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes)
{
    auto *interface = static_cast<PointerDeviceConfigurationV1 *>(sender());
    auto *seatConfig = seatUserConfig(interface->wSeat());

    if (changes.testFlag(PointerDeviceConfigurationV1::ScrollFactorChanged)) {
        seatConfig->setPointerScrollFactor(interface->scrollFactor());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)) {
        seatConfig->setTouchpadAccelSpeed(interface->accelSpeed());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)) {
        seatConfig->setTouchpadNaturalScroll(interface->naturalScroll());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::SendEventsModeChanged)) {
        seatConfig->setTouchpadSendEventsMode(interface->sendEventsMode().toInt());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::DisableWhileTypingChanged)) {
        seatConfig->setTouchpadDisableWhileTyping(interface->disableWhileTyping());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::TapToClickChanged)) {
        seatConfig->setTouchpadTapToClick(interface->tapToClick());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged))
        seatConfig->setTouchpadAccelerationProfile(interface->accelerationProfile());
    Q_EMIT settingsChanged(interface->wSeat());
}

void InputManager::onKeyboardSettingsApplied(KeyboardSettingsInterfaceV1::ChangeFlags changes)
{
    KeyboardSettingsInterfaceV1 *interface =
        static_cast<KeyboardSettingsInterfaceV1 *>(sender());
    auto *seatConfig = seatUserConfig(interface->wSeat());

    if (!seatConfig->isInitializeSucceeded()) {
        interface->sendFailed();
        return;
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::NumLockChanged)) {
        seatConfig->setKeyboardNumLock(interface->numLock());
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::RepeatChanged)) {
        seatConfig->setKeyboardDelay(interface->repeatDelay());
        seatConfig->setKeyboardRate(interface->repeatRate());
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::XkbRulesChanged)) {
        seatConfig->setXkbLayout(interface->xkbLayout());
        seatConfig->setXkbModel(interface->xkbModel());
        seatConfig->setXkbVariant(interface->xkbVariant());
        seatConfig->setXkbOptions(interface->xkbOptions());
    }

    Q_EMIT settingsChanged(interface->wSeat());
}

void InputManager::applyNumLockToKeyboards()
{
    const auto seats = m_seats();
    for (WSeat *seat : seats) {
        auto *config = seatUserConfig(seat);
        if (configurationReady(config))
            setNumLockForSeat(seat, config->keyboardNumLock());
    }
}

void InputManager::applyXkbConfigForSeat(WSeat *seat)
{
    if (!m_seats().contains(seat)) {
        return;
    }

    auto *seatConfig = seatUserConfig(seat);

    applyXkbConfig(seat,
                   seatConfig->xkbLayout(),
                   seatConfig->xkbModel(),
                   seatConfig->xkbVariant(),
                   seatConfig->xkbOptions());
}

void InputManager::applyXkbConfig(WSeat *seat,
                                  const QString &layoutName,
                                  const QString &modelName,
                                  const QString &variantName,
                                  const QString &optionsName)
{
    Q_ASSERT(seat);

    struct xkb_rule_names rules = {};
    QByteArray layout = layoutName.toUtf8();
    QByteArray model = modelName.toUtf8();
    QByteArray variant = variantName.toUtf8();
    QByteArray options = optionsName.toUtf8();
    rules.layout = layout.constData();
    rules.model = model.constData();
    rules.variant = variant.constData();
    rules.options = options.constData();

    seat->setXkbRuleNames(rules);

    applyNumLockToKeyboards();
}

void InputManager::setNumLockForSeat(WSeat *seat, bool enabled)
{
    if (!seat)
        return;

    const auto devices = seat->deviceList();
    for (auto *device : devices) {
        if (device->type() == WInputDevice::Type::Keyboard)
            setNumLockForDevice(device, enabled);
    }

    setNumLockForDevice(seat->keyboardGroupKeyboard(), enabled);
}

void InputManager::setNumLockForDevice(WInputDevice *device, bool enabled)
{
    if (!device || device->type() != WInputDevice::Type::Keyboard)
        return;

    if (device->handle()->type != WLR_INPUT_DEVICE_KEYBOARD)
        return;
    auto *wlrKeyboard = wlr_keyboard_from_input_device(device->handle());
    if (!wlrKeyboard || !wlrKeyboard->keymap || !wlrKeyboard->xkb_state)
        return;

    xkb_mod_index_t numlock = xkb_keymap_mod_get_index(wlrKeyboard->keymap, XKB_MOD_NAME_NUM);
    if (numlock == XKB_MOD_INVALID)
        return;

    xkb_mod_mask_t locked = xkb_state_serialize_mods(wlrKeyboard->xkb_state, XKB_STATE_MODS_LOCKED);
    if (enabled) {
        locked |= (1u << numlock);
    } else {
        locked &= ~(1u << numlock);
    }
    const auto depressed = xkb_state_serialize_mods(wlrKeyboard->xkb_state, XKB_STATE_MODS_DEPRESSED);
    const auto latched = xkb_state_serialize_mods(wlrKeyboard->xkb_state, XKB_STATE_MODS_LATCHED);
    const auto group = xkb_state_serialize_layout(wlrKeyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
    wlr_keyboard_notify_modifiers(wlrKeyboard, depressed, latched, locked, group);
}

void InputManager::onInputAssigned(WInputDevice *input)
{
    auto *seat = input->seat();
    Q_ASSERT(seat);
    auto *seatConfig = seatUserConfig(seat);

    if (!configurationReady(seatConfig)) {
        connect(seatConfig, &SeatUserDConfig::configInitializeSucceed,
                this, &InputManager::refresh, Qt::UniqueConnection);
        connect(seatConfig, &SeatUserDConfig::configInitializeFailed,
                this, &InputManager::refresh, Qt::UniqueConnection);
        return;
    }

    if (input->type() == WInputDevice::Type::Keyboard) {
        if (input->handle()->type == WLR_INPUT_DEVICE_KEYBOARD) {
            auto *keyboard = wlr_keyboard_from_input_device(input->handle());
            wlr_keyboard_set_repeat_info(keyboard, seatConfig->keyboardRate(), seatConfig->keyboardDelay());
        }
        setNumLockForSeat(seat, seatConfig->keyboardNumLock());
    }

    if (!wlr_input_device_is_libinput(input->handle())) {
        return;
    }

    struct libinput_device *inputDevice = wlr_libinput_get_device_handle(input->handle());
    struct udev_device *udevDevice = libinput_device_get_udev_device(inputDevice);
    bool leftHanded = (seatConfig->pointerHandMode() == "Left");


    if (udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
        configLeftHanded(inputDevice, leftHanded);
        configAccelSpeed(inputDevice, seatConfig->mouseAccelSpeed());
        configAccelProfile(inputDevice, static_cast<libinput_config_accel_profile>(seatConfig->mouseAccelerationProfile()));
        configNaturalScroll(inputDevice, seatConfig->mouseNaturalScroll());
    }

    if (udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
        configLeftHanded(inputDevice, leftHanded);
        configAccelSpeed(inputDevice, seatConfig->touchpadAccelSpeed());
        configAccelProfile(inputDevice, static_cast<libinput_config_accel_profile>(seatConfig->touchpadAccelerationProfile()));
        configNaturalScroll(inputDevice, seatConfig->touchpadNaturalScroll());
        configSendEventsMode(inputDevice, seatConfig->touchpadSendEventsMode());
        configDwtEnabled(inputDevice, seatConfig->touchpadDisableWhileTyping()
                                             ? LIBINPUT_CONFIG_DWT_ENABLED
                                             : LIBINPUT_CONFIG_DWT_DISABLED);
        configTapEnabled(inputDevice, seatConfig->touchpadTapToClick()
                                         ? LIBINPUT_CONFIG_TAP_ENABLED
                                         : LIBINPUT_CONFIG_TAP_DISABLED);
    }
}

void InputManager::onSeatRemoved(WSeat *seat)
{
    auto *config = seatUserConfig(seat);
    disconnect(config, nullptr, this, nullptr);
}

bool InputManager::hardwareAttached() const { return m_attached; }

void InputManager::setGestureManager(GestureManager *gestures)
{
    Q_ASSERT(!m_gestures);
    m_gestures = gestures;
}
