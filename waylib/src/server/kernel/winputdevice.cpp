// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputdevice.h"
#include "wseat.h"
#include "private/wglobal_p.h"

#include <qwinputdevice.h>

#include <QDebug>
#include <QInputDevice>
#include <QPointer>

#include <private/qpointingdevice_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

// Input device management and events
Q_LOGGING_CATEGORY(waylibInput, "waylib.server.input", QtInfoMsg)

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
        if (handle()) {
            qCDebug(waylibInput) << "Releasing input device:" 
                                << QString::fromUtf8(nativeHandle()->name);
            handle()->set_data(nullptr, nullptr);
            if (seat)
                seat->detachInputDevice(q_func());
        }
    }

    WWRAP_HANDLE_FUNCTIONS(qw_input_device, wlr_input_device)

    W_DECLARE_PUBLIC(WInputDevice)

    QPointer<QInputDevice> qtDevice;
    QPointer<QObject> hoverTarget;
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
    case WLR_INPUT_DEVICE_TABLET: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    qCWarning(waylibInput) << "Unknown input device type:" << d->nativeHandle()->type 
                          << "from device:" << QString::fromUtf8(d->nativeHandle()->name);
    return Type::Unknow;
}

void WInputDevice::setSeat(WSeat *seat)
{
    W_D(WInputDevice);
    if (d->seat != seat) {
        qCDebug(waylibInput) << "Input device" << QString::fromUtf8(d->nativeHandle()->name) 
                            << "assigned to seat:" << (seat ? seat->name() : QString("(null)"));
        d->seat = seat;
    }
}

WSeat *WInputDevice::seat() const
{
    W_DC(WInputDevice);
    return d->seat;
}

void WInputDevice::setQtDevice(QInputDevice *device)
{
    W_D(WInputDevice);
    if (d->qtDevice != device) {
        qCDebug(waylibInput) << "Qt device" << (device ? device->name() : QString("(null)"))
                            << "associated with input device:" 
                            << QString::fromUtf8(d->nativeHandle()->name);
        d->qtDevice = device;
    }
}

QInputDevice *WInputDevice::qtDevice() const
{
    W_DC(WInputDevice);
    return d->qtDevice;
}

void WInputDevice::setExclusiveGrabber(QObject *grabber)
{
    W_D(WInputDevice);
    auto pointerDevice = qobject_cast<QPointingDevice*>(d->qtDevice);
    if (!pointerDevice) {
        qCDebug(waylibInput) << "Cannot set exclusive grabber: device is not a pointing device";
        return;
    }
    auto dd = QPointingDevicePrivate::get(pointerDevice);
    if (dd->activePoints.isEmpty()) {
        qCDebug(waylibInput) << "Cannot set exclusive grabber: no active points";
        return;
    }
    auto firstPoint = dd->activePoints.values().first();
    qCDebug(waylibInput) << "Setting exclusive grabber" << grabber 
                         << "for device:" << QString::fromUtf8(d->nativeHandle()->name);
    dd->setExclusiveGrabber(nullptr, firstPoint.eventPoint, grabber);
}

QObject *WInputDevice::exclusiveGrabber() const
{
    W_DC(WInputDevice);
    auto pointerDevice = qobject_cast<QPointingDevice*>(d->qtDevice);
    if (!pointerDevice)
        return nullptr;
    auto dd = QPointingDevicePrivate::get(pointerDevice);
    return dd->firstPointExclusiveGrabber();
}

QObject *WInputDevice::hoverTarget() const
{
    W_DC(WInputDevice);
    return d->hoverTarget;
}

void WInputDevice::setHoverTarget(QObject *object)
{
    W_D(WInputDevice);
    d->hoverTarget = object;
}

WAYLIB_SERVER_END_NAMESPACE
