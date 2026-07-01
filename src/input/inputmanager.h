// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/input-manager/inputmanagerinterfacev1.h"
Q_MOC_INCLUDE("seatuserconfig.hpp")

#include <wayland-server-core.h>

#include <qwinputdevice.h>

#include <QObject>
#include <QMap>

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
    void handleMousePointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes);
    void handleTouchpadPointerConfigApplied(PointerDeviceConfigurationV1::ChangeFlags changes);
    void handleKeyboardSettingsApplied(KeyboardSettingsInterfaceV1::ChangeFlags changes);
    void onInputAdded(WInputDevice *input);
    void onConfigInitializeSucceed();

private:
    SeatUserDConfig* m_seatDConfig = nullptr;
};
