// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputdevice.h"
#include "wseat.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

extern "C" {
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
}

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QInputDevice>
#include <QPointer>
#include <QScopeGuard>
#include <QRegularExpression>

#include <private/qpointingdevice_p.h>

#include <libudev.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

using InputDeviceRegistry = QHash<const wlr_input_device *, WInputDevice *>;
Q_GLOBAL_STATIC(InputDeviceRegistry, s_inputDevices)

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

class Q_DECL_HIDDEN WInputDevicePrivate : public WWrapObjectPrivate
{
public:
    WInputDevicePrivate(WInputDevice *qq, wlr_input_device *handle, bool _isVirtual)
        : WWrapObjectPrivate(qq)
        , handle(handle)
        , isVirtual(_isVirtual)
    {
        Q_ASSERT(handle);
        Q_ASSERT(!s_inputDevices->contains(handle));
        s_inputDevices->insert(handle, qq);
        wl_list_init(&destroy.link);
        wl_list_init(&key.link);
        wl_list_init(&modifiers.link);
        destroy.notify = handleDestroy;
        wl_signal_add(&handle->events.destroy, &destroy);
        if (handle->type == WLR_INPUT_DEVICE_KEYBOARD) {
            auto *keyboard = wlr_keyboard_from_input_device(handle);
            key.notify = handleKey;
            wl_signal_add(&keyboard->events.key, &key);
            modifiers.notify = handleModifiers;
            wl_signal_add(&keyboard->events.modifiers, &modifiers);
        }
    }

    void instantRelease() override {
        if (handle) {
            qCDebug(lcWlInput) << "Releasing input device:" 
                                << QString::fromUtf8(handle->name);
            if (seat)
                seat->detachInputDevice(q_func());
            s_inputDevices->remove(handle);
            if (!wl_list_empty(&destroy.link)) {
                wl_list_remove(&destroy.link);
                wl_list_init(&destroy.link);
            }
            for (wl_listener *listener : { &key, &modifiers }) {
                if (!wl_list_empty(&listener->link)) {
                    wl_list_remove(&listener->link);
                    wl_list_init(&listener->link);
                }
            }
            handle = nullptr;
        }
    }

    static void handleDestroy(wl_listener *listener, [[maybe_unused]] void *data)
    {
        WInputDevicePrivate *self;
        self = wl_container_of(listener, self, destroy);
        self->q_func()->safeDeleteLater();
    }

    static void handleKey(wl_listener *listener, void *data)
    {
        WInputDevicePrivate *self;
        self = wl_container_of(listener, self, key);
        Q_EMIT self->q_func()->keyboardKey(static_cast<wlr_keyboard_key_event *>(data));
    }

    static void handleModifiers(wl_listener *listener, [[maybe_unused]] void *data)
    {
        WInputDevicePrivate *self;
        self = wl_container_of(listener, self, modifiers);
        Q_EMIT self->q_func()->keyboardModifiers();
    }

    W_DECLARE_PUBLIC(WInputDevice)

    QPointer<QInputDevice> qtDevice;
    QPointer<QObject> hoverTarget;
    wlr_input_device *handle = nullptr;
    wl_listener destroy;
    wl_listener key;
    wl_listener modifiers;
    WSeat *seat = nullptr;
    bool isVirtual = false;
};

WInputDevice::WInputDevice(wlr_input_device *handle, bool isVirtual)
    : WWrapObject(*new WInputDevicePrivate(this, handle, isVirtual))
{

}

wlr_input_device *WInputDevice::handle() const
{
    W_DC(WInputDevice);
    return d->handle;
}

WInputDevice *WInputDevice::fromHandle(const wlr_input_device *handle)
{
    return s_inputDevices->value(handle);
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

    switch (d->handle->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: return Type::Keyboard;
    case WLR_INPUT_DEVICE_POINTER: return Type::Pointer;
    case WLR_INPUT_DEVICE_TOUCH: return Type::Touch;
    case WLR_INPUT_DEVICE_TABLET: return Type::Tablet;
    case WLR_INPUT_DEVICE_TABLET_PAD: return Type::TabletPad;
    case WLR_INPUT_DEVICE_SWITCH: return Type::Switch;
    }

    qCWarning(lcWlInput) << "Unknown input device type:" << d->handle->type
                          << "from device:" << QString::fromUtf8(d->handle->name);
    return Type::Unknow;
}

QString WInputDevice::name() const
{
    W_DC(WInputDevice);

    if (d->handle && d->handle->name) {
        return QString::fromUtf8(d->handle->name);
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
        qCDebug(lcWlInput) << "Input device" << QString::fromUtf8(d->handle->name)
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
                            << QString::fromUtf8(d->handle->name);
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
    if (d->handle && wlr_input_device_is_libinput(d->handle)) {
        if (auto libinputDevice = wlr_libinput_get_device_handle(d->handle)) {
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
                         << "for device:" << QString::fromUtf8(d->handle->name);
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
