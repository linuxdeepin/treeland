// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputdevice.h"
#include "wseat.h"

#include <qwinputdevice.h>

#include <QDebug>
#include <QInputDevice>

extern "C" {
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WInputDevicePrivate : public WObjectPrivate
{
public:
    WInputDevicePrivate(WInputDevice *qq, void *handle)
        : WObjectPrivate(qq)
        , handle(reinterpret_cast<QWInputDevice*>(handle))
    {
        this->handle->setData(this, qq);
    }
    ~WInputDevicePrivate() {
        handle->setData(this, nullptr);
        if (seat)
            seat->detachInputDevice(q_func());
    }

    inline wlr_input_device *nativeHandle() const {
        Q_ASSERT(handle);
        return handle->handle();
    }

    W_DECLARE_PUBLIC(WInputDevice);

    QWInputDevice *handle = nullptr;
    QPointer<QInputDevice> qtDevice;
    WSeat *seat = nullptr;
};

WInputDevice::WInputDevice(QWInputDevice *handle)
    : WObject(*new WInputDevicePrivate(this, handle))
{

}

QWInputDevice *WInputDevice::handle() const
{
    W_DC(WInputDevice);
    return reinterpret_cast<QWInputDevice*>(d->handle);
}

WInputDevice *WInputDevice::fromHandle(const QWInputDevice *handle)
{
    return handle->getData<WInputDevice>();
}

WInputDevice *WInputDevice::from(const QInputDevice *device)
{
    if (device->systemId() < 65536)
        return nullptr;
    return reinterpret_cast<WInputDevice*>(device->systemId());
}

WInputDevice::Type WInputDevice::type() const
{
    W_DC(WInputDevice);

    switch (d->handle->handle()->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return Type::Keyboard;
    case WLR_INPUT_DEVICE_POINTER: return Type::Pointer;
    case WLR_INPUT_DEVICE_TOUCH: return Type::Touch;
    case WLR_INPUT_DEVICE_TABLET_TOOL: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    // TODO: use qCWarning
    qWarning("Unknow input device type %i\n", d->handle->handle()->type);
    return Type::Unknow;
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

void WInputDevice::setQtDevice(QInputDevice *device)
{
    W_D(WInputDevice);
    d->qtDevice = device;
}

QInputDevice *WInputDevice::qtDevice() const
{
    W_DC(WInputDevice);
    return d->qtDevice;
}

WAYLIB_SERVER_END_NAMESPACE
