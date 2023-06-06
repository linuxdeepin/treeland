// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QObject>

QT_BEGIN_NAMESPACE
class QInputDevice;
QT_END_NAMESPACE

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

    template<class QInputDevice>
    inline QInputDevice *qtDevice() const {
        return qobject_cast<QInputDevice*>(qtDevice());
    }
    QInputDevice *qtDevice() const;
    static WInputDevice *from(const QInputDevice *device);

    Type type() const;
    void setSeat(WSeat *seat);
    WSeat *seat() const;

private:
    friend class QWlrootsIntegration;
    void setQtDevice(QInputDevice *device);
};

WAYLIB_SERVER_END_NAMESPACE
