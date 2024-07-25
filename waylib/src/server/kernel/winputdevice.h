// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QObject>
#include <qwglobal.h>

QW_BEGIN_NAMESPACE
class qw_input_device;
QW_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QInputDevice;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevicePrivate;
class WAYLIB_SERVER_EXPORT WInputDevice : public WWrapObject
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

    WInputDevice(QW_NAMESPACE::qw_input_device *handle);

    QW_NAMESPACE::qw_input_device *handle() const;

    static WInputDevice *fromHandle(const QW_NAMESPACE::qw_input_device *handle);

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
