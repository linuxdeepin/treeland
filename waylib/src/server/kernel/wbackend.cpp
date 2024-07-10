// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbackend.h"
#include "woutput.h"
#include "wserver.h"
#include "winputdevice.h"
#include "platformplugin/qwlrootsintegration.h"
#include "platformplugin/qwlrootscreen.h"
#include "private/wglobal_p.h"

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwinputdevice.h>

#include <QDebug>

extern "C" {
#include <wlr/backend.h>
#define static
#include <wlr/types/wlr_output.h>
#undef static
#include <wlr/types/wlr_input_device.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WBackendPrivate : public WObjectPrivate
{
public:
    WBackendPrivate(WBackend *qq)
        : WObjectPrivate(qq)
    {

    }

    inline QWBackend *handle() const {
        return q_func()->nativeInterface<QWBackend>();
    }

    inline wlr_backend *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    // begin slot function
    void on_new_output(QWOutput *output);
    void on_new_input(QWInputDevice *device);
    void on_input_destroy(QWInputDevice *data);
    void on_output_destroy(QWOutput *output);
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WBackend)

    QVector<WOutput*> outputList;
    QVector<WInputDevice*> inputList;

    struct Keyboard {
        Keyboard(WBackendPrivate *self, wlr_input_device *d)
            : self(self), device(d) {}

        WBackendPrivate *self;
        wlr_input_device *device;

        wl_listener modifiers;
        wl_listener key;
    };
};

void WBackendPrivate::on_new_output(QWOutput *output)
{
    W_Q(WBackend);
    auto woutput = new WOutput(output, q);

    outputList << woutput;
    QWlrootsIntegration::instance()->addScreen(woutput);

    woutput->safeConnect(&QWOutput::beforeDestroy, q, [this, output] {
        on_output_destroy(output);
    });

    Q_EMIT q->outputAdded(woutput);
}

void WBackendPrivate::on_new_input(QWInputDevice *device)
{
    W_Q(WBackend);
    auto input_device = new WInputDevice(device);
    inputList << input_device;
    input_device->safeConnect(&QWInputDevice::beforeDestroy, q, [this, device] {
        on_input_destroy(device);
    });

    Q_EMIT q->inputAdded(input_device);
}

void WBackendPrivate::on_input_destroy(QWInputDevice *data)
{
    for (int i = 0; i < inputList.count(); ++i) {
        if (inputList.at(i)->handle() == data) {
            auto device = inputList.takeAt(i);

            W_Q(WBackend);
            Q_EMIT q->inputRemoved(device);
            device->safeDeleteLater();
            return;
        }
    }
}

void WBackendPrivate::on_output_destroy(QWOutput *output)
{
    for (int i = 0; i < outputList.count(); ++i) {
        if (outputList.at(i)->handle() == output) {
            auto woutput = outputList.takeAt(i);

            W_Q(WBackend);
            Q_EMIT q->outputRemoved(woutput);
            QWlrootsIntegration::instance()->removeScreen(woutput);
            woutput->safeDeleteLater();
            return;
        }
    }
}

void WBackendPrivate::connect()
{
    QObject::connect(handle(), &QWBackend::newOutput, q_func(), [this] (QWOutput *output) {
        on_new_output(output);
    });
    QObject::connect(handle(), &QWBackend::newInput, q_func(), [this] (QWInputDevice *device) {
        on_new_input(device);
    });
}

WBackend::WBackend()
    : WObject(*new WBackendPrivate(this))
{

}

QWBackend *WBackend::handle() const
{
    return nativeInterface<QWBackend>();
}

QVector<WOutput*> WBackend::outputList() const
{
    W_DC(WBackend);
    return d->outputList;
}

QVector<WInputDevice *> WBackend::inputDeviceList() const
{
    W_DC(WBackend);
    return d->inputList;
}

template<class T>
static bool hasBackend(QWBackend *handle)
{
    if (qobject_cast<T*>(handle))
        return true;
    if (auto multiBackend = qobject_cast<QWMultiBackend*>(handle)) {
        bool exists = false;
        multiBackend->forEachBackend([] (wlr_backend *backend, void *userData) {
            bool &exists = *reinterpret_cast<bool*>(userData);
            if (T::from(backend))
                exists = true;
        }, &exists);

        return exists;
    }

    return false;
}

bool WBackend::hasDrm() const
{
    return hasBackend<QWDrmBackend>(handle());
}

bool WBackend::hasX11() const
{
    return hasBackend<QWX11Backend>(handle());
}

bool WBackend::hasWayland() const
{
    return hasBackend<QWWaylandBackend>(handle());
}

void WBackend::create(WServer *server)
{
    W_D(WBackend);

    if (!m_handle) {
        m_handle = QWBackend::autoCreate(server->handle());
        Q_ASSERT(m_handle);
    }

    d->connect();
}

void WBackend::destroy(WServer *server)
{
    Q_UNUSED(server)
    W_D(WBackend);

    qDeleteAll(d->inputList);
    qDeleteAll(d->outputList);
    d->inputList.clear();
    d->outputList.clear();
    m_handle = nullptr;
}

wl_global *WBackend::global() const
{
    return nullptr;
}

QByteArrayView WBackend::interfaceName() const
{
    return {};
}

WAYLIB_SERVER_END_NAMESPACE
