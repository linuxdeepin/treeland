// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/input-manager/inputmanagerinterfacev1.h"
Q_MOC_INCLUDE("seatuserconfig.hpp")

#include <wayland-server-core.h>

#include <functional>
#include <QtQml/qqmlregistration.h>
#include <QObject>
#include <QString>

#include <wseat.h>

class SeatUserDConfig;
class SeatManager;
class GestureManager;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WBackend;
WAYLIB_SERVER_END_NAMESPACE

class InputManager : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    using ConfigProvider = std::function<SeatUserDConfig *(const QString &)>;
    using SeatProvider = std::function<QList<WSeat *>()>;
    InputManager(ConfigProvider configuration, SeatProvider seats, SeatManager *hardware,
                 WAYLIB_SERVER_NAMESPACE::WBackend *backend,
                 QObject *parent = nullptr);
    bool hardwareAttached() const;
    void setGestureManager(GestureManager *gestures);
    ~InputManager() override;

    static void setNumLockForDevice(WInputDevice *device, bool enabled);

    void refresh();

Q_SIGNALS:
    void settingsChanged(WSeat *seat);

public:
    void activate();
    void deactivate();

public Q_SLOTS:
    void onMouseSettingsCreated(MouseSettingsInterfaceV1 *interface);
    void onTouchpadSettingsCreated(TouchpadSettingsInterfaceV1 *interface);
    void onKeyboardSettingsCreated(KeyboardSettingsInterfaceV1 *interface);
    void onMousePointerConfigCreated(PointerDeviceConfigurationV1 *config);
    void onTouchpadPointerConfigCreated(PointerDeviceConfigurationV1 *config);

private Q_SLOTS:
    void onMousePointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes);
    void onTouchpadPointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes);
    void onKeyboardSettingsApplied(KeyboardSettingsInterfaceV1::ChangeFlags changes);
    void onInputAssigned(WInputDevice *input);
    void onSeatRemoved(WSeat *seat);

private:
    const ConfigProvider m_configuration;
    const SeatProvider m_seats;
    SeatManager *const m_hardware;
    WAYLIB_SERVER_NAMESPACE::WBackend *const m_backend;
    GestureManager *m_gestures = nullptr;
    bool m_attached = false;
    bool initializeKeyboardSettings(KeyboardSettingsInterfaceV1 *interface);
    SeatUserDConfig *seatUserConfig(WSeat *seat) const;
    void setupSeat(WSeat *seat);
    void applyNumLockToKeyboards();
    void applyXkbConfigForSeat(WSeat *seat);
    void applyXkbConfig(WSeat *seat,
                        const QString &layout,
                        const QString &model,
                        const QString &variant,
                        const QString &options);
    static void setNumLockForSeat(WSeat *seat, bool enabled);

};
