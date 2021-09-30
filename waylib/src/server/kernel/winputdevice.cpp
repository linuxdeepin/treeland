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
#include "winputdevice.h"
#include "wseat.h"

#include <QDebug>

extern "C" {
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WInputDevicePrivate : public WObjectPrivate
{
public:
    WInputDevicePrivate(WInputDevice *qq, void *handle)
        : WObjectPrivate(qq)
        , handle(reinterpret_cast<wlr_input_device*>(handle))
    {
        this->handle->data = qq;
    }
    ~WInputDevicePrivate() {
        handle->data = nullptr;
        if (seat)
            seat->detachInputDevice(q_func());
    }

    W_DECLARE_PUBLIC(WInputDevice);

    wlr_input_device *handle = nullptr;
    WInputDevice::Device *device = nullptr;
    WSeat *seat = nullptr;
};

WInputDevice::WInputDevice(WInputDeviceHandle *handle)
    : WObject(*new WInputDevicePrivate(this, handle))
{

}

WInputDeviceHandle *WInputDevice::handle() const
{
    W_DC(WInputDevice);
    return reinterpret_cast<WInputDeviceHandle*>(d->handle);
}

WInputDevice *WInputDevice::fromHandle(const WInputDeviceHandle *handle)
{
    auto wlr_device = reinterpret_cast<const wlr_input_device*>(handle);
    return reinterpret_cast<WInputDevice*>(wlr_device->data);
}

WInputDevice::Type WInputDevice::type() const
{
    W_DC(WInputDevice);

    switch (d->handle->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return Type::Keyboard;
    case WLR_INPUT_DEVICE_POINTER: return Type::Pointer;
    case WLR_INPUT_DEVICE_TOUCH: return Type::Touch;
    case WLR_INPUT_DEVICE_TABLET_TOOL: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    // TODO: use qCWarning
    qWarning("Unknow input device type %i\n", d->handle->type);
    return Type::Unknow;
}

WInputDevice::Device *WInputDevice::device() const
{
    W_DC(WInputDevice);
    return d->device;
}

WInputDevice::DeviceHandle *WInputDevice::deviceHandle() const
{
    W_DC(WInputDevice);
    return reinterpret_cast<WInputDevice::DeviceHandle*>(d->handle->_device);
}

void WInputDevice::setSeat(WSeat *seat)
{
    W_D(WInputDevice);
    d->seat = seat;
}

WSeat *WInputDevice::seat() const
{
    W_DC(WInputDevice);
    return d->seat;
}

WAYLIB_SERVER_END_NAMESPACE
