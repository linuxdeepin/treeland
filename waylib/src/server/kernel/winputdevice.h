/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>
#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDeviceHandle;
class WInputDevicePrivate;
class WInputDevice : public WObject
{
    Q_GADGET
    W_DECLARE_PRIVATE(WInputDevice)
public:
    enum class Type {
        Unknow,
        Keyboard,
        Pointer,
        Touch,
        Tablet,
        TabletPad,
        Switch
    };
    Q_ENUM(Type)

    // wl_pointer_button_state
    enum class ButtonState {
        Released = 0,
        Pressed = 1
    };
    Q_ENUM(ButtonState)

    // wl_keyboard_key_state
    enum class KeyState {
        Released = 0,
        Pressed = 1
    };
    Q_ENUM(KeyState)

    enum class AxisSource {
        Wheel,
        Finger,
        Continuous,
        WheelTilt,
    };
    Q_ENUM(AxisSource)

    struct DeviceHandle;
    struct Device {};
    struct Keyboard : public Device {

    };
    struct Pointer : public Device {

    };
    struct Touch : public Device {

    };
    struct Tablet : public Device {

    };
    struct TabletPad : public Device {

    };
    struct Switch : public Device {

    };

    WInputDevice(WInputDeviceHandle *handle);

    WInputDeviceHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    static WInputDevice *fromHandle(const WInputDeviceHandle *handle);
    template<typename DNativeInterface>
    static inline WInputDevice *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const WInputDeviceHandle*>(handle));
    }

    Type type() const;
    Device *device() const;
    template<typename T>
    T *device() const {
        return static_cast<T*>(device());
    }

    DeviceHandle *deviceHandle() const;
    template<typename DNativeInterface>
    DNativeInterface *deviceNativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(deviceHandle());
    }

    void setSeat(WSeat *seat);
    WSeat *seat() const;
};

WAYLIB_SERVER_END_NAMESPACE
