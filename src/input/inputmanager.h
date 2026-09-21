// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/input-manager/inputmanagerinterfacev1.h"
Q_MOC_INCLUDE("seatuserconfig.hpp")

#include <wayland-server-core.h>

#include <QObject>
#include <QString>

#include <wseat.h>

class SeatUserDConfig;

class InputManager : public QObject
{
    Q_OBJECT
public:
    explicit InputManager(QObject *parent = nullptr);
    ~InputManager() override;

    static void setNumLockForDevice(WInputDevice *device, bool enabled);

    void setupSeatUserConfig(const QString &userName);

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

    QString m_userName;
};
