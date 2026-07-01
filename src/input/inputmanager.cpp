// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputmanager.h"
#include "seatuserconfig.hpp"
#include "helper.h"
#include "inputdevice.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "seat/seatsmanager.h"
#include "session/session.h"
#include "xsettings/settingmanager.h"

#include <libinput.h>

#include <qwseat.h>

#include <wbackend.h>
#include <wcursor.h>
#include <winputdevice.h>


InputManager::InputManager(QObject *parent)
    : QObject(parent)
{
}

InputManager::~InputManager()
{
    if (m_seatDConfig) {
        delete m_seatDConfig;
    }
}

void InputManager::setupSeatUserConfig(const QString &userName)
{
    if (m_seatDConfig) {
        delete m_seatDConfig;
    }

    m_seatDConfig = SeatUserDConfig::createByName("org.deepin.dde.treeland.user.seat",
                                                   "org.deepin.dde.treeland",
                                                   "/" + userName);

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (m_seatDConfig->isInitializeSucceeded()) {
#else
    if (m_seatDConfig->isInitializeSucceed()) {
#endif
        onConfigInitializeSucceed();
    } else {
        connect(m_seatDConfig,
                &SeatUserDConfig::configInitializeSucceed,
                this,
                &InputManager::onConfigInitializeSucceed,
                Qt::SingleShotConnection);
    }
}

void InputManager::onConfigInitializeSucceed()
{
    auto seatMgr = Helper::instance()->seatManager();
    if (seatMgr) {
        for (auto *seat : seatMgr->seats()) {
            if (auto *cursor = seat->cursor())
                cursor->setScrollFactor(m_seatDConfig->pointerScrollFactor());
        }
    }

    auto backend = Helper::instance()->backend();
    connect(backend,
            &WBackend::inputAdded,
            this,
            &InputManager::onInputAdded);
    const auto inputDevices = backend->inputDeviceList();
    for (WInputDevice *device : inputDevices) {
        onInputAdded(device);
    }
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
    if (!m_seatDConfig)
        return;

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        return;
    }

    KeyboardSettingsInterfaceV1::FeatureFlags features;
    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (!device->handle()->is_libinput()) {
            continue;
        }

        if (device->type() != WInputDevice::Type::Keyboard)
            continue;

        auto *keyboard = qobject_cast<qw_keyboard *>(device->handle());
        if (!keyboard)
            continue;

        auto *wlrKeyboard = keyboard->handle();
        if (!wlrKeyboard || !wlrKeyboard->keymap)
            continue;

        if (xkb_map_mod_get_index(wlrKeyboard->keymap, XKB_MOD_NAME_NUM) != XKB_MOD_INVALID) {
            features.setFlag(KeyboardSettingsInterfaceV1::NumLock);
            break;
        }
    }

    interface->sendFeature(features, true);
    interface->sendNumLock(m_seatDConfig->keyboardNumLock(), true);
    interface->sendRepeat(m_seatDConfig->keyboardRate(), m_seatDConfig->keyboardDelay(), true);
    interface->sendDone();

    connect(interface,
            &KeyboardSettingsInterfaceV1::applied,
            this,
            &InputManager::handleKeyboardSettingsApplied);
}

void InputManager::onMousePointerConfigCreated(PointerDeviceConfigurationV1 *config)
{
    if (!m_seatDConfig)
        return;

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        return;
    }

    PointerDeviceConfigurationV1::FeatureFlags features;
    features.setFlag(PointerDeviceConfigurationV1::ScrollFactor);
    features.setFlag(PointerDeviceConfigurationV1::NaturalScroll);
    features.setFlag(PointerDeviceConfigurationV1::EventsMode);

    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (!device->handle()->is_libinput()) {
            continue;
        }

        if (device->type() != WInputDevice::Type::Pointer) {
            continue;
        }

        struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle()->handle());
        struct udev_device *udevDevice =
            libinput_device_get_udev_device(inputDevice);
        if (!udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
            continue;
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
    config->sendScrollFactor(m_seatDConfig->pointerScrollFactor(), true);

    auto handModeStr = m_seatDConfig->pointerHandMode();
    auto handMode = (handModeStr == "Left")
        ? PointerDeviceConfigurationV1::Left
        : PointerDeviceConfigurationV1::Right;
    config->sendHandedMode(handMode, true);

    config->sendAccelSpeed(m_seatDConfig->mouseAccelSpeed(), true);
    config->sendAccelerationProfile(static_cast<PointerDeviceConfigurationV1::AccelerationProfile>(m_seatDConfig->mouseAccelerationProfile()), true);
    config->sendNaturalScroll(m_seatDConfig->mouseNaturalScroll(), true);
    config->sendDone(0);

    connect(config,
            &PointerDeviceConfigurationV1::applied,
            this,
            &InputManager::handleMousePointerConfigApplied);
}

void InputManager::handleMousePointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes)
{
    auto *interface = static_cast<PointerDeviceConfigurationV1 *>(sender());

    if (!m_seatDConfig) {
        interface->sendFailed();
        return;
    }

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        interface->sendFailed();
        return;
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::ScrollFactorChanged)) {
        m_seatDConfig->setPointerScrollFactor(interface->scrollFactor());
        if (auto *cursor = interface->wSeat()->cursor())
            cursor->setScrollFactor(interface->scrollFactor());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::HandedModeChanged)) {
        m_seatDConfig->setPointerHandMode(interface->handedMode() == PointerDeviceConfigurationV1::Left
                                              ? QStringLiteral("Left")
                                              : QStringLiteral("Right"));
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)) {
        m_seatDConfig->setMouseAccelSpeed(interface->accelSpeed());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged)) {
        m_seatDConfig->setMouseAccelerationProfile(interface->accelerationProfile());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)) {
        m_seatDConfig->setMouseNaturalScroll(interface->naturalScroll());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)
        || changes.testFlag(PointerDeviceConfigurationV1::AccelerationProfileChanged)
        || changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)
        || changes.testFlag(PointerDeviceConfigurationV1::HandedModeChanged)) {
        const auto devices = interface->wSeat()->deviceList();
        for (WInputDevice *device : devices) {
            if (!device->handle()->is_libinput()) {
                continue;
            }

            if (device->type() != WInputDevice::Type::Pointer) {
                continue;
            }

            struct udev_device *udevDevice =
                libinput_device_get_udev_device(wlr_libinput_get_device_handle(device->handle()->handle()));
            if (!udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
                continue;
            }

            struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle()->handle());
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
    features.setFlag(PointerDeviceConfigurationV1::NaturalScroll);
    features.setFlag(PointerDeviceConfigurationV1::EventsMode);

    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (!device->handle()->is_libinput()) {
            continue;
        }

        if (device->type() != WInputDevice::Type::Pointer) {
            continue;
        }

        struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle()->handle());
        struct udev_device *udevDevice =
            libinput_device_get_udev_device(inputDevice);
        if (!udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
            continue;
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

    if (!m_seatDConfig)
        return;

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        return;
    }

    config->sendFeature(features, true);
    config->sendScrollFactor(m_seatDConfig->pointerScrollFactor(), true);
    config->sendAccelSpeed(m_seatDConfig->touchpadAccelSpeed(), true);
    config->sendAccelerationProfile(static_cast<PointerDeviceConfigurationV1::AccelerationProfile>(m_seatDConfig->touchpadAccelerationProfile()), true);
    config->sendNaturalScroll(m_seatDConfig->touchpadNaturalScroll(), true);
    config->sendSendEventsMode(PointerDeviceConfigurationV1::SendEventsModes::fromInt(m_seatDConfig->touchpadSendEventsMode()), true);
    config->sendDisableWhileTyping(m_seatDConfig->touchpadDisableWhileTyping(), true);
    config->sendTapToClick(m_seatDConfig->touchpadTapToClick(), true);
    config->sendDone(0);

    connect(config,
            &PointerDeviceConfigurationV1::applied,
            this,
            &InputManager::handleTouchpadPointerConfigApplied);
}

void InputManager::handleTouchpadPointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes)
{
    auto *interface = static_cast<PointerDeviceConfigurationV1 *>(sender());

    if (!m_seatDConfig) {
        interface->sendFailed();
        return;
    }

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        interface->sendFailed();
        return;
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::ScrollFactorChanged)) {
        m_seatDConfig->setPointerScrollFactor(interface->scrollFactor());
        if (auto *cursor = interface->wSeat()->cursor())
            cursor->setScrollFactor(interface->scrollFactor());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::AccelSpeedChanged)) {
        m_seatDConfig->setTouchpadAccelSpeed(interface->accelSpeed());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::NaturalScrollChanged)) {
        m_seatDConfig->setTouchpadNaturalScroll(interface->naturalScroll());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::SendEventsModeChanged)) {
        m_seatDConfig->setTouchpadSendEventsMode(interface->sendEventsMode().toInt());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::DisableWhileTypingChanged)) {
        m_seatDConfig->setTouchpadDisableWhileTyping(interface->disableWhileTyping());
    }

    if (changes.testFlag(PointerDeviceConfigurationV1::TapToClickChanged)) {
        m_seatDConfig->setTouchpadTapToClick(interface->tapToClick());
    }

    const auto devices = interface->wSeat()->deviceList();
    for (WInputDevice *device : devices) {
        if (!device->handle()->is_libinput())
            continue;

        if (device->type() != WInputDevice::Type::Pointer) {
            continue;
        }

        struct udev_device *udevDevice =
            libinput_device_get_udev_device(wlr_libinput_get_device_handle(device->handle()->handle()));

        if (!udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
            continue;
        }

        struct libinput_device *inputDevice = wlr_libinput_get_device_handle(device->handle()->handle());
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

void InputManager::handleKeyboardSettingsApplied(KeyboardSettingsInterfaceV1::ChangeFlags changes)
{
    KeyboardSettingsInterfaceV1 *interface =
        static_cast<KeyboardSettingsInterfaceV1 *>(sender());
    if (!m_seatDConfig) {
        interface->sendFailed();
        return;
    }

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        interface->sendFailed();
        return;
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::NumLockChanged)) {
        m_seatDConfig->setKeyboardNumLock(interface->numLock());
    }

    if (changes.testFlag(KeyboardSettingsInterfaceV1::RepeatChanged)) {
        m_seatDConfig->setKeyboardDelay(interface->repeatDelay());
        m_seatDConfig->setKeyboardRate(interface->repeatRate());
    }

    auto *keyboardDevice = interface->wSeat()->keyboard();
    if (keyboardDevice) {
        auto *keyboard = qobject_cast<qw_keyboard *>(keyboardDevice->handle());
        if (keyboard) {
            if (changes.testFlag(KeyboardSettingsInterfaceV1::RepeatChanged)) {
                keyboard->set_repeat_info(interface->repeatRate(), interface->repeatDelay());
            }
        }
    }

    const auto devices = interface->wSeat()->deviceList();
    for (WInputDevice *device : devices) {
        if (device->type() != WInputDevice::Type::Keyboard)
            continue;

        if (changes.testFlag(KeyboardSettingsInterfaceV1::NumLockChanged)) {
            setNumLockForDevice(device, interface->numLock());
        }
    }
}

void InputManager::setNumLockForDevice(WInputDevice *device, bool enabled)
{
    if (!device || device->type() != WInputDevice::Type::Keyboard)
        return;

    auto *keyboard = qobject_cast<qw_keyboard *>(device->handle());
    if (!keyboard)
        return;
    auto *wlrKeyboard = keyboard->handle();
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
    xkb_state_update_mask(wlrKeyboard->xkb_state,
                          xkb_state_serialize_mods(wlrKeyboard->xkb_state, XKB_STATE_MODS_DEPRESSED),
                          xkb_state_serialize_mods(wlrKeyboard->xkb_state, XKB_STATE_MODS_LATCHED),
                          locked, 0, 0, 0);

    uint32_t leds = wlrKeyboard->leds;
    if (enabled) {
        leds |= WLR_LED_NUM_LOCK;
    } else {
        leds &= ~WLR_LED_NUM_LOCK;
    }
    wlr_keyboard_led_update(wlrKeyboard, leds);
}

void InputManager::onInputAdded(WInputDevice *input)
{
    if (!m_seatDConfig)
        return;

#if SEATUSERDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (!m_seatDConfig->isInitializeSucceeded()) {
#else
    if (!m_seatDConfig->isInitializeSucceed()) {
#endif
        return;
    }

    if (!input->handle()->is_libinput()) {
        return;
    }

    struct libinput_device *inputDevice = wlr_libinput_get_device_handle(input->handle()->handle());
    struct udev_device *udevDevice = libinput_device_get_udev_device(inputDevice);
    bool leftHanded = (m_seatDConfig->pointerHandMode() == "Left");

    if (input->type() == WInputDevice::Type::Keyboard) {
        if (auto *keyboard = qobject_cast<qw_keyboard *>(input->handle())) {
            keyboard->set_repeat_info(m_seatDConfig->keyboardRate(), m_seatDConfig->keyboardDelay());
        }
        setNumLockForDevice(input, m_seatDConfig->keyboardNumLock());
    }

    if (udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
        configLeftHanded(inputDevice, leftHanded);
        configAccelSpeed(inputDevice, m_seatDConfig->mouseAccelSpeed());
        configAccelProfile(inputDevice, static_cast<libinput_config_accel_profile>(m_seatDConfig->mouseAccelerationProfile()));
        configNaturalScroll(inputDevice, m_seatDConfig->mouseNaturalScroll());
    }

    if (udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
        configLeftHanded(inputDevice, leftHanded);
        configAccelSpeed(inputDevice, m_seatDConfig->touchpadAccelSpeed());
        configAccelProfile(inputDevice, static_cast<libinput_config_accel_profile>(m_seatDConfig->touchpadAccelerationProfile()));
        configNaturalScroll(inputDevice, m_seatDConfig->touchpadNaturalScroll());
        configSendEventsMode(inputDevice, m_seatDConfig->touchpadSendEventsMode());
        configDwtEnabled(inputDevice, m_seatDConfig->touchpadDisableWhileTyping()
                                             ? LIBINPUT_CONFIG_DWT_ENABLED
                                             : LIBINPUT_CONFIG_DWT_DISABLED);
        configTapEnabled(inputDevice, m_seatDConfig->touchpadTapToClick()
                                         ? LIBINPUT_CONFIG_TAP_ENABLED
                                         : LIBINPUT_CONFIG_TAP_DISABLED);
    }
}
