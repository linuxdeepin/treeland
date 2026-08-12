// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputdevice.h"
#include "wseat.h"
#include "wscoplistener.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <wlr_all.h>
#include <QDebug>
#include <QFile>
#include <QInputDevice>
#include <QPointer>
#include <QScopeGuard>
#include <QRegularExpression>

#include <private/qpointingdevice_p.h>

#include <libudev.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

// DeviceInfoParser implementation
DeviceInfoParser& DeviceInfoParser::instance()
{
    static DeviceInfoParser parser;
    return parser;
}

void DeviceInfoParser::refreshDeviceInfo()
{
    QMutexLocker locker(&m_mutex);
    m_deviceMap.clear();

    QFile procFile("/proc/bus/input/devices");
    if (!procFile.open(QIODevice::ReadOnly)) {
        return;
    }

    QString content = procFile.readAll();
    QStringList blocks = content.split("\n\n", Qt::SkipEmptyParts);

    for (const QString& block : std::as_const(blocks)) {
        parseDeviceBlock(block.trimmed());
    }
}

void DeviceInfoParser::parseDeviceBlock(const QString& block)
{
    static const QRegularExpression nameRegex("Name=\"([^\"]+)\"");
    ProcDeviceInfo info;
    QStringList lines = block.split('\n');

    for (const QString& line : std::as_const(lines)) {
        if (line.startsWith("N: Name=")) {
            auto match = nameRegex.match(line);
            if (match.hasMatch()) {
                info.name = match.captured(1);
            }
        }
        else if (line.startsWith("P: Phys=")) {
            info.physPath = line.mid(8);
        }
    }

    if (info.isValid()) {
        m_deviceMap[info.name] = info;
    }
}

QString DeviceInfoParser::getPhysicalPath(const QString& deviceName)
{
    QMutexLocker locker(&m_mutex);
    if (!m_deviceMap.contains(deviceName)) {
        locker.unlock();
        refreshDeviceInfo();
        locker.relock();
    }

    return m_deviceMap.value(deviceName).physPath;
}

class Q_DECL_HIDDEN WInputDevicePrivate : public WObjectPrivate
{
public:
    WInputDevicePrivate(WInputDevice *qq, wlr_input_device *handle, bool _isVirtual)
        : WObjectPrivate(qq)
        , isVirtual(_isVirtual)
        , m_handle(handle)
    {
        Q_ASSERT(handle);
        handle->data = qq;
    }

    inline wlr_input_device *handle() const {
        return m_handle;
    }

    W_DECLARE_PUBLIC(WInputDevice)

    QPointer<QInputDevice> qtDevice;
    QPointer<QObject> hoverTarget;
    WSeat *seat = nullptr;
    bool isVirtual = false;

private:
    // The backend owns this handle and destroys it after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_input_device *m_handle = nullptr;
};

WInputDevice::WInputDevice(wlr_input_device *handle, bool isVirtual)
    : QObject(nullptr)
    , WObject(*new WInputDevicePrivate(this, handle, isVirtual))
{
}

WInputDevice::~WInputDevice()
{
    teardown();
    W_D(WInputDevice);
    // Clear the reverse fromHandle() mapping while the native handle is
    // still alive (owner teardown deletes this wrapper before the native
    // device is destroyed, or from its destroy callback).
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    // Detach from the seat this device was attached to (owner teardown;
    // the native handle is still alive here).
    if (d->seat)
        d->seat->detachInputDevice(this);
}

wlr_input_device *WInputDevice::handle() const
{
    W_DC(WInputDevice);
    return d->handle();
}

WInputDevice *WInputDevice::fromHandle(wlr_input_device *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WInputDevice*>(handle->data);
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

    switch (d->handle()->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return Type::Keyboard;
    case WLR_INPUT_DEVICE_POINTER: return Type::Pointer;
    case WLR_INPUT_DEVICE_TOUCH: return Type::Touch;
    case WLR_INPUT_DEVICE_TABLET: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    qCWarning(lcWlInput) << "Unknown input device type:" << d->handle()->type
                          << "from device:" << QString::fromUtf8(d->handle()->name);
    return Type::Unknow;
}

QString WInputDevice::name() const
{
    W_DC(WInputDevice);

    if (d->handle() && d->handle()->name) {
        return QString::fromUtf8(d->handle()->name);
    }

    if (d->qtDevice) {
        return d->qtDevice->name();
    }

    return QString();
}

void WInputDevice::setSeat(WSeat *seat)
{
    W_D(WInputDevice);
    if (d->seat != seat) {
        qCDebug(lcWlInput) << "Input device" << QString::fromUtf8(d->handle()->name)
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
        qCDebug(lcWlInput) << "Qt device" << (device ? device->name() : QString("(null)"))
                            << "associated with input device:" 
                            << QString::fromUtf8(d->handle()->name);
        d->qtDevice = device;
    }
}

QInputDevice *WInputDevice::qtDevice() const
{
    W_DC(WInputDevice);
    return d->qtDevice;
}

QString WInputDevice::devicePath() const
{
    W_DC(WInputDevice);
    if (d->handle() && wlr_input_device_is_libinput(d->handle())) {
        if (auto libinputDevice = wlr_libinput_get_device_handle(d->handle())) {
            if (auto udevDevice = libinput_device_get_udev_device(libinputDevice)) {
                auto deviceGuard = qScopeGuard([udevDevice] { udev_device_unref(udevDevice); });

                const char* physPath = udev_device_get_property_value(udevDevice, "PHYS");
                if (physPath) {
                    return QString::fromUtf8(physPath);
                }
                const char* devPath = udev_device_get_property_value(udevDevice, "DEVPATH");
                if (devPath) {
                    QString fullDevPath = QString::fromUtf8(devPath);
                    static const QRegularExpression usbRegex(
                        QStringLiteral("/devices/pci\\d+:\\d+/(\\d+:\\d+:\\d+\\.\\d+)/usb\\d+/1-\\d+/1-(\\d+\\.\\d+)/"));
                    auto match = usbRegex.match(fullDevPath);
                    if (match.hasMatch()) {
                        return QString("usb-%1-%2/input0").arg(match.captured(1)).arg(match.captured(2));
                    }
                }
            }
        }
    }
    QString deviceName = name();
    QString procPhysPath = DeviceInfoParser::instance().getPhysicalPath(deviceName);
    if (!procPhysPath.isEmpty()) {
        return procPhysPath;
    }
    return QString();
}

bool WInputDevice::isVirtual() const
{
    W_DC(WInputDevice);

    return d->isVirtual;
}

WInputDevice::LibinputPointerType WInputDevice::libinputPointerType() const
{
    if (!wlr_input_device_is_libinput(handle())) {
        return LibinputPointerType::Unknown;
    }

    struct libinput_device *inputDevice = wlr_libinput_get_device_handle(handle());
    struct udev_device *udevDevice = libinput_device_get_udev_device(inputDevice);

    if (udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE")) {
        return LibinputPointerType::Mouse;
    }

    if (udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD")) {
        return LibinputPointerType::TouchPad;
    }

    return LibinputPointerType::Unknown;
}

void WInputDevice::setExclusiveGrabber(QObject *grabber)
{
    W_D(WInputDevice);
    auto pointerDevice = qobject_cast<QPointingDevice*>(d->qtDevice);
    if (!pointerDevice) {
        qCDebug(lcWlInput) << "Cannot set exclusive grabber: device is not a pointing device";
        return;
    }
    auto dd = QPointingDevicePrivate::get(pointerDevice);
    if (dd->activePoints.isEmpty()) {
        qCDebug(lcWlInput) << "Cannot set exclusive grabber: no active points";
        return;
    }
    auto firstPoint = dd->activePoints.values().first();
    qCDebug(lcWlInput) << "Setting exclusive grabber" << grabber 
                         << "for device:" << QString::fromUtf8(d->handle()->name);
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
