// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <QObject>
#include <QMap>
#include <QMutex>

QT_BEGIN_NAMESPACE
class QInputDevice;
class QEventPoint;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevicePrivate;
class WBackendPrivate;
class WInputMethodHelper;

// Internal helper for parsing /proc/bus/input/devices
struct ProcDeviceInfo {
    QString name;
    QString physPath;

    bool isValid() const {
        return !name.isEmpty() && !physPath.isEmpty();
    }
};

class DeviceInfoParser {
public:
    static DeviceInfoParser& instance();

    QString getPhysicalPath(const QString& deviceName);

private:
    DeviceInfoParser() = default;
    void refreshDeviceInfo();
    void parseDeviceBlock(const QString& block);

    QMap<QString, ProcDeviceInfo> m_deviceMap;
    QMutex m_mutex;
};
class WAYLIB_SERVER_EXPORT WInputDevice : public QObject, public WObject
{
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

    enum class LibinputPointerType {
        Unknown,
        Mouse,
        TouchPad,
    };
    Q_ENUM(LibinputPointerType)

    WInputDevice(wlr_input_device *handle, bool isVirtual = false);
    ~WInputDevice() override;

    wlr_input_device *handle() const;

    static WInputDevice *fromHandle(wlr_input_device *handle);

    template<class QInputDevice>
    inline QInputDevice *qtDevice() const {
        return qobject_cast<QInputDevice*>(qtDevice());
    }
    QInputDevice *qtDevice() const;
    static WInputDevice *from(const QInputDevice *device);

    Type type() const;
    QString name() const;
    void setSeat(WSeat *seat);
    WSeat *seat() const;
    QString devicePath() const;

    bool isVirtual() const;

    LibinputPointerType libinputPointerType() const;
private:
    friend class QWlrootsIntegration;
    friend class WSeat;
    friend class WSeatPrivate;
    friend class WBackendPrivate;
    friend class WInputMethodHelper;
    void setQtDevice(QInputDevice *device);
    QObject *exclusiveGrabber() const;
    void setExclusiveGrabber(QObject *grabber);

    QObject *hoverTarget() const;
    void setHoverTarget(QObject *object);

    // Owned by the backend/seat: released with `delete` from the native
    // destroy callback, never with deleteLater().
    using QObject::deleteLater;
};

WAYLIB_SERVER_END_NAMESPACE
