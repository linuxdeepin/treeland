// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputdevice.h"
#include "wseat.h"
#include "private/wglobal_p.h"

#include <qwinputdevice.h>

#include <QDebug>
#include <QInputDevice>
#include <QPointer>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WInputDevicePrivate : public WWrapObjectPrivate
{
public:
    WInputDevicePrivate(WInputDevice *qq, void *handle)
        : WWrapObjectPrivate(qq)
    {
        initHandle(reinterpret_cast<qw_input_device*>(handle));
        this->handle()->set_data(this, qq);
    }

    void instantRelease() override {
        handle()->set_data(nullptr, nullptr);
        if (seat)
            seat->detachInputDevice(q_func());
    }

    WWRAP_HANDLE_FUNCTIONS(qw_input_device, wlr_input_device)

    W_DECLARE_PUBLIC(WInputDevice);

    QPointer<QInputDevice> qtDevice;
    WSeat *seat = nullptr;
};

WInputDevice::WInputDevice(qw_input_device *handle)
    : WWrapObject(*new WInputDevicePrivate(this, handle))
{

}

qw_input_device *WInputDevice::handle() const
{
    W_DC(WInputDevice);
    return d->handle();
}

WInputDevice *WInputDevice::fromHandle(const qw_input_device *handle)
{
    return handle->get_data<WInputDevice>();
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

    switch (d->nativeHandle()->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return Type::Keyboard;
    case WLR_INPUT_DEVICE_POINTER: return Type::Pointer;
    case WLR_INPUT_DEVICE_TOUCH: return Type::Touch;
    case WLR_INPUT_DEVICE_TABLET_TOOL: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    // TODO: use qCWarning
    qWarning("Unknow input device type %i\n", d->nativeHandle()->type);
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
