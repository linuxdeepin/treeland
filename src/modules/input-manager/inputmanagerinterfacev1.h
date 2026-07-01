// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wserver.h>

#include <wayland-server-core.h>

#include <QObject>
#include <QInputDevice>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class PointerDeviceConfigurationV1;
class MouseSettingsInterfaceV1;
class TouchpadSettingsInterfaceV1;
class KeyboardSettingsInterfaceV1;
class TreelandInputManagerInterfaceV1Private;

class TreelandInputManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    enum class DeviceType {
        Unknown = 0,
        Mouse = 1 << 0,
        TouchPad = 1 << 1,
        Keyboard = 1 << 2,
    };
    Q_DECLARE_FLAGS(DeviceTypes, DeviceType)
    Q_FLAG(DeviceTypes)

    explicit TreelandInputManagerInterfaceV1(QObject *parent = nullptr);
    ~TreelandInputManagerInterfaceV1() override;

    void sendCapabilityAvailable(TreelandInputManagerInterfaceV1::DeviceTypes types);
    void sendCapabilityUnavailable(TreelandInputManagerInterfaceV1::DeviceTypes types);

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void mouseSettingsCreated(MouseSettingsInterfaceV1 *interface);
    void touchpadSettingsCreated(TouchpadSettingsInterfaceV1 *interface);
    void keyboardSettingsCreated(KeyboardSettingsInterfaceV1 *interface);

private Q_SLOTS:
    void onInputAdded(WInputDevice *input);
    void onInputRemoved(WInputDevice *input);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    TreelandInputManagerInterfaceV1::DeviceTypes inputDeviceListTypes() const;
    TreelandInputManagerInterfaceV1::DeviceTypes inputDeviceType(WInputDevice *input) const;

private:
    friend class TreelandInputManagerInterfaceV1Private;
    std::unique_ptr<TreelandInputManagerInterfaceV1Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(TreelandInputManagerInterfaceV1::DeviceTypes)

class PointerDeviceConfigurationV1Private;
class PointerDeviceConfigurationV1 : public QObject
{
    Q_OBJECT
public:
    ~PointerDeviceConfigurationV1() override;

    enum FeatureFlag {
        NoFeature = 0,
        ScrollFactor = 1 << 0,
        HandMode = 1 << 1,
        AccelSpeed = 1 << 2,
        AccelProfile = 1 << 3,
        EventsMode = 1 << 4,
        NaturalScroll = 1 << 5,
        DisableWhileTyping = 1 << 6,
        TapToClick = 1 << 7,
    };
    Q_DECLARE_FLAGS(FeatureFlags, FeatureFlag)
    Q_FLAG(FeatureFlags)

    enum ChangeFlag {
        NoChange = 0,
        ScrollFactorChanged = 1 << 0,
        HandedModeChanged = 1 << 1,
        AccelSpeedChanged = 1 << 2,
        AccelerationProfileChanged = 1 << 3,
        SendEventsModeChanged = 1 << 4,
        NaturalScrollChanged = 1 << 5,
        DisableWhileTypingChanged = 1 << 6,
        TapToClickChanged = 1 << 7,
    };
    Q_DECLARE_FLAGS(ChangeFlags, ChangeFlag)
    Q_FLAG(ChangeFlags)

    enum HandedMode {
        Right,
        Left,
    };

    enum AccelerationProfile {
        None = 0,
        Flat = 1 << 0,
        Adaptive = 1 << 1,
        Custom = 1 << 2,
    };

    enum SendEventsMode {
        Enabled = 0,
        Disabled = 1 << 0,
        DisabledOnExternalMouse = 1 << 1,
    };
    Q_DECLARE_FLAGS(SendEventsModes, SendEventsMode)
    Q_FLAG(SendEventsModes)

    void sendFeature(FeatureFlags features, bool force = false);
    void sendScrollFactor(double factor, bool force = false);
    void sendHandedMode(HandedMode mode, bool force = false);
    void sendAccelSpeed(double speed, bool force = false);
    void sendAccelerationProfile(AccelerationProfile profile, bool force = false);
    void sendSendEventsMode(SendEventsModes mode, bool force = false);
    void sendNaturalScroll(bool enable, bool force = false);
    void sendDisableWhileTyping(bool enable, bool force = false);
    void sendTapToClick(bool enable, bool force = false);
    void sendFailed();
    void sendDone(uint32_t serial);

    double scrollFactor() const;
    HandedMode handedMode() const;
    double accelSpeed() const;
    AccelerationProfile accelerationProfile() const;
    SendEventsModes sendEventsMode() const;
    bool naturalScroll() const;
    bool disableWhileTyping() const;
    bool tapToClick() const;

    wl_resource *seat() const;
    WSeat *wSeat() const;

Q_SIGNALS:
    void applied(ChangeFlags changes);

private:
    explicit PointerDeviceConfigurationV1(wl_resource *resource,
                                          wl_resource *seat,
                                          QObject *parent = nullptr);

private:
    friend class MouseSettingsInterfaceV1Private;
    friend class TouchpadSettingsInterfaceV1Private;
    std::unique_ptr<PointerDeviceConfigurationV1Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(PointerDeviceConfigurationV1::FeatureFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(PointerDeviceConfigurationV1::ChangeFlags)

class MouseSettingsInterfaceV1Private;
class MouseSettingsInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    ~MouseSettingsInterfaceV1() override;

    wl_resource *seat() const;
    WSeat *wSeat() const;

Q_SIGNALS:
    void pointerDeviceConfigurationCreated(PointerDeviceConfigurationV1 *config);

private:
    explicit MouseSettingsInterfaceV1(wl_resource *resource,
                                      wl_resource *seat,
                                      QObject *parent = nullptr);

private:
    friend class TreelandInputManagerInterfaceV1Private;
    std::unique_ptr<MouseSettingsInterfaceV1Private> d;
};

class TouchpadSettingsInterfaceV1Private;
class TouchpadSettingsInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    ~TouchpadSettingsInterfaceV1() override;

    wl_resource *seat() const;
    WSeat *wSeat() const;

Q_SIGNALS:
    void pointerDeviceConfigurationCreated(PointerDeviceConfigurationV1 *config);

private:
    explicit TouchpadSettingsInterfaceV1(wl_resource *resource,
                                         wl_resource *seat,
                                         QObject *parent = nullptr);

private:
    friend class TreelandInputManagerInterfaceV1Private;
    std::unique_ptr<TouchpadSettingsInterfaceV1Private> d;
};

class KeyboardSettingsInterfaceV1Private;
class KeyboardSettingsInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    ~KeyboardSettingsInterfaceV1() override;

    enum FeatureFlag {
        NoFeature = 0,
        NumLock = 1 << 0,
    };
    Q_DECLARE_FLAGS(FeatureFlags, FeatureFlag)
    Q_FLAG(FeatureFlags)

    enum ChangeFlag {
        NoChange = 0,
        RepeatChanged = 1 << 0,
        NumLockChanged = 1 << 1,
    };
    Q_DECLARE_FLAGS(ChangeFlags, ChangeFlag)
    Q_FLAG(ChangeFlags)

    void sendFeature(FeatureFlags features, bool force = false);
    void sendRepeat(int32_t rate, int32_t delay, bool force = false);
    void sendNumLock(bool enabled, bool force = false);
    void sendFailed();
    void sendDone();

    int32_t repeatRate() const;
    int32_t repeatDelay() const;
    bool numLock() const;

    wl_resource *seat() const;
    WSeat *wSeat() const;

Q_SIGNALS:
    void applied(ChangeFlags changes);

private:
    explicit KeyboardSettingsInterfaceV1(wl_resource *resource,
                                         wl_resource *seat,
                                         QObject *parent = nullptr);

private:
    friend class TreelandInputManagerInterfaceV1Private;
    std::unique_ptr<KeyboardSettingsInterfaceV1Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardSettingsInterfaceV1::FeatureFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardSettingsInterfaceV1::ChangeFlags)
