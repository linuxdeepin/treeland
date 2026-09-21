// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputmanager.h"
#include "core/dconfigmanager.h"
#include "seatuserconfig.hpp"
#include "treelandconfig.hpp"
#include "helper.h"
#include "inputdevice.h"
#include "common/treelandlogging.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "seat/seatsmanager.h"
#include "session/session.h"
#include "xsettings/settingmanager.h"

#include <libinput.h>
#include <xkbcommon/xkbcommon.h>

#include <wlr_all.h>

#include <wcursor.h>
#include <winputdevice.h>
#include <wseat.h>
#include <wbackend.h>

namespace {

bool isTreelandConfigInitialized(TreelandConfig *config)
{
    if (!config)
        return false;

#if TREELANDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    return config->isInitializeSucceeded();
#else
    return config->isInitializeSucceed();
#endif
}

}

InputManager::InputManager(QObject *parent)
    : QObject(parent)
{
}

InputManager::~InputManager()
{
}

void InputManager::setupSeatUserConfig(const QString &userName)
{
    auto *seatManager = Helper::instance()->seatManager();
    auto *configManager = DConfigManager::instance();
    const auto seats = seatManager->seats();
    if (!m_userName.isEmpty()) {
        for (WSeat *seat : seats) {
            auto *config = configManager->userSeatConfig(m_userName, seat->name());
            disconnect(config, nullptr, this, nullptr);
        }
    }
    m_userName = userName;

    for (WSeat *seat : seats)
        setupSeat(seat);

    connect(seatManager,
            &SeatsManager::seatAdded,
            this,
            &InputManager::setupSeat,
            Qt::UniqueConnection);
    connect(seatManager,
            &SeatsManager::seatRemoved,
            this,
            &InputManager::onSeatRemoved,
            Qt::UniqueConnection);
    connect(seatManager,
            &SeatsManager::deviceAssigned,
            this,
            &InputManager::onInputAssigned,
            Qt::UniqueConnection);
}

SeatUserDConfig *InputManager::seatUserConfig(WSeat *seat) const
{
    Q_ASSERT(!m_userName.isEmpty());
    auto *config = DConfigManager::instance()->userSeatConfig(m_userName, seat->name());
    Q_ASSERT(config);

    return config;
}

void InputManager::setupSeat(WSeat *seat)
{
    Q_ASSERT(!m_userName.isEmpty());

    auto *config = seatUserConfig(seat);
    if (auto *cursor = seat->cursor())
        cursor->setScrollFactor(config->pointerScrollFactor());

    InputDevice::instance()->setHoldTimeout(config->touchpadHoldTimeoutMs());

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

    auto *globalConfig = Helper::instance()->globalConfig();
    if (!isTreelandConfigInitialized(globalConfig)) {
        return false;
    }

    KeyboardSettingsInterfaceV1::FeatureFlags features;
    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
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
    interface->sendNumLock(globalConfig->keyboardNumLock(), true);
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
    auto *seatConfig = seatUserConfig(config->wSeat());

    PointerDeviceConfigurationV1::FeatureFlags features;
    features.setFlag(PointerDeviceConfigurationV1::ScrollFactor);
    features.setFlag(PointerDeviceConfigurationV1::EventsMode);

    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
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
        if (auto *cursor = interface->wSeat()->cursor())
            cursor->setScrollFactor(interface->scrollFactor());
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

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)
        || changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged)
        || changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)
        || changes.testFlag(PointerDeviceConfigurationV1::HandedModeChanged)) {
        const auto devices = interface->wSeat()->deviceList();
        for (WInputDevice *device : devices) {
            if (!wlr_input_device_is_libinput(device->handle())) {
                continue;
            }

            if (device->type() != WInputDevice::Type::Pointer) {
                continue;
            }

            struct udev_device *udevDevice =
                libinput_device_get_udev_device(wlr_libinput_get_device_handle(device->handle()));
            if (!udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
                continue;
            }

            struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle());
            if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged))
                configAccelSpeed(inputDevice, interface->accelSpeed());
            if (changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged))
                configAccelProfile(inputDevice, static_cast<libinput_config_accel_profile>(interface->accelerationProfile()));
            if (changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged))
                configNaturalScroll(inputDevice, interface->naturalScroll());

            if (changes.testFlag(PointerDeviceConfigurationV1::HandedModeChanged)) {
                bool leftHanded = (interface->handedMode() == PointerDeviceConfigurationV1::Left);
                configLeftHanded(inputDevice, leftHanded);
            }
        }
    }
}

void InputManager::onTouchpadPointerConfigCreated(PointerDeviceConfigurationV1 *config)
{
    PointerDeviceConfigurationV1::FeatureFlags features;
    features.setFlag(PointerDeviceConfigurationV1::ScrollFactor);
    features.setFlag(PointerDeviceConfigurationV1::EventsMode);

    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
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
        if (auto *cursor = interface->wSeat()->cursor())
            cursor->setScrollFactor(interface->scrollFactor());
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

    const auto devices = interface->wSeat()->deviceList();
    for (WInputDevice *device : devices) {
        if (!wlr_input_device_is_libinput(device->handle()))
            continue;

        if (device->type() != WInputDevice::Type::Pointer) {
            continue;
        }

        struct udev_device *udevDevice =
            libinput_device_get_udev_device(wlr_libinput_get_device_handle(device->handle()));

        if (!udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
            continue;
        }

        struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle());
        if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)) {
            configAccelSpeed(inputDevice, interface->accelSpeed());
        }

        if (changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged)) {
            configAccelProfile(inputDevice,
                               static_cast<libinput_config_accel_profile>(interface->accelerationProfile()));
        }

        if (changes.testFlag(PointerDeviceConfigurationV1::SendEventsModeChanged)) {
            configSendEventsMode(inputDevice, interface->sendEventsMode().toInt());
        }

        if (changes.testFlag(PointerDeviceConfigurationV1::DisableWhileTypingChanged)) {
            configDwtEnabled(inputDevice, interface->disableWhileTyping()
                             ? LIBINPUT_CONFIG_DWT_ENABLED
                             : LIBINPUT_CONFIG_DWT_DISABLED);
        }

        if (changes.testFlag(PointerDeviceConfigurationV1::TapToClickChanged)) {
            configTapEnabled(inputDevice, interface->tapToClick()
                             ? LIBINPUT_CONFIG_TAP_ENABLED
                             : LIBINPUT_CONFIG_TAP_DISABLED);
        }

        if (changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)) {
            configNaturalScroll(inputDevice, interface->naturalScroll());
        }
    }
}

void InputManager::onKeyboardSettingsApplied(KeyboardSettingsInterfaceV1::ChangeFlags changes)
{
    KeyboardSettingsInterfaceV1 *interface =
        static_cast<KeyboardSettingsInterfaceV1 *>(sender());
    auto *seatConfig = seatUserConfig(interface->wSeat());

    if (!isTreelandConfigInitialized(Helper::instance()->globalConfig())) {
        interface->sendFailed();
        return;
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::NumLockChanged)) {
        Helper::instance()->globalConfig()->setKeyboardNumLock(interface->numLock());
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

    auto *keyboardDevice = interface->wSeat()->keyboardGroupKeyboard();
    if (keyboardDevice) {
        if (keyboardDevice->handle()->type == WLR_INPUT_DEVICE_KEYBOARD) {
            auto *keyboard = wlr_keyboard_from_input_device(keyboardDevice->handle());
            if (changes.testFlag(KeyboardSettingsInterfaceV1::RepeatChanged)) {
                wlr_keyboard_set_repeat_info(keyboard, interface->repeatRate(), interface->repeatDelay());
            }
        }
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::NumLockChanged))
        setNumLockForSeat(interface->wSeat(), interface->numLock());

    if (changes.testFlag(KeyboardSettingsInterfaceV1::XkbRulesChanged)) {
        applyXkbConfig(interface->wSeat(),
                       interface->xkbLayout(),
                       interface->xkbModel(),
                       interface->xkbVariant(),
                       interface->xkbOptions());
    }
}

void InputManager::applyNumLockToKeyboards()
{
    auto *globalConfig = Helper::instance()->globalConfig();
    if (!isTreelandConfigInitialized(globalConfig))
        return;

    auto *seatManager = Helper::instance()->seatManager();
    if (seatManager) {
        const auto seats = seatManager->seats();
        for (WSeat *seat : seats) {
            setNumLockForSeat(seat, globalConfig->keyboardNumLock());
        }
    }
}

void InputManager::applyXkbConfigForSeat(WSeat *seat)
{
    auto *seatManager = Helper::instance()->seatManager();
    if (!seatManager->seats().contains(seat)) {
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

    if (!wlr_input_device_is_libinput(input->handle())) {
        return;
    }

    struct libinput_device *inputDevice = wlr_libinput_get_device_handle(input->handle());
    struct udev_device *udevDevice = libinput_device_get_udev_device(inputDevice);
    bool leftHanded = (seatConfig->pointerHandMode() == "Left");

    if (input->type() == WInputDevice::Type::Keyboard) {
        if (input->handle()->type == WLR_INPUT_DEVICE_KEYBOARD) {
            auto *keyboard = wlr_keyboard_from_input_device(input->handle());
            wlr_keyboard_set_repeat_info(keyboard, seatConfig->keyboardRate(), seatConfig->keyboardDelay());
        }
        if (isTreelandConfigInitialized(Helper::instance()->globalConfig())) {
            if (seat)
                setNumLockForSeat(seat, Helper::instance()->globalConfig()->keyboardNumLock());
        }
    }

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
