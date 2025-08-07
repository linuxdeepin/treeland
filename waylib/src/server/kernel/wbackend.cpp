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
#include <qwsession.h>

#include <QDebug>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WBackendPrivate : public WObjectPrivate
{
public:
    WBackendPrivate(WBackend *qq)
        : WObjectPrivate(qq)
    {

    }

    inline qw_backend *handle() const {
        return q_func()->nativeInterface<qw_backend>();
    }

    inline wlr_backend *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    // begin slot function
    void on_new_output(wlr_output *output);
    void on_new_input(wlr_input_device *device);
    void on_input_destroy(qw_input_device *data);
    void on_output_destroy(qw_output *output);
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

private:
    qw_session *session = nullptr;
};

void WBackendPrivate::on_new_output(wlr_output *output)
{
    W_Q(WBackend);
    auto qoutput = qw_output::from(output);
    auto woutput = new WOutput(qoutput, q);

    outputList << woutput;
    QWlrootsIntegration::instance()->addScreen(woutput);

    woutput->safeConnect(&qw_output::before_destroy, q, [this, qoutput] {
        on_output_destroy(qoutput);
    });

    Q_EMIT q->outputAdded(woutput);
}

void WBackendPrivate::on_new_input(wlr_input_device *device)
{
    W_Q(WBackend);
    auto qinput_device = qw_input_device::from(device);
    auto winput_device = new WInputDevice(qinput_device);
    inputList << winput_device;
    winput_device->safeConnect(&qw_input_device::before_destroy, q, [this, qinput_device] {
        on_input_destroy(qinput_device);
    });

    Q_EMIT q->inputAdded(winput_device);
}

void WBackendPrivate::on_input_destroy(qw_input_device *data)
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

void WBackendPrivate::on_output_destroy(qw_output *output)
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
    QObject::connect(handle(), &qw_backend::notify_new_output, q_func(), [this] (wlr_output *output) {
        on_new_output(output);
    });
    QObject::connect(handle(), &qw_backend::notify_new_input, q_func(), [this] (wlr_input_device *device) {
        on_new_input(device);
    });
}

WBackend::WBackend()
    : WObject(*new WBackendPrivate(this))
{

}

qw_backend *WBackend::handle() const
{
    return nativeInterface<qw_backend>();
}

qw_session *WBackend::session() const
{
    W_DC(WBackend);
    return d->session;
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
static bool hasBackend(qw_backend *handle)
{
    if (qobject_cast<T*>(handle))
        return true;
    if (auto multiBackend = qobject_cast<qw_multi_backend*>(handle)) {
        bool exists = false;
        multiBackend->for_each_backend([] (wlr_backend *backend, void *userData) {
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
    return hasBackend<qw_drm_backend>(handle());
}

bool WBackend::hasX11() const
{
    return hasBackend<qw_x11_backend>(handle());
}

bool WBackend::hasWayland() const
{
    return hasBackend<qw_wayland_backend>(handle());
}

void WBackend::create(WServer *server)
{
    W_D(WBackend);

    if (!m_handle) {
        wlr_session *session = nullptr;
        m_handle = qw_backend::autocreate(server->handle()->get_event_loop(), &session);
        Q_ASSERT(m_handle);
        d->session = qw_session::from(session);
    }

    d->connect();
}

void WBackend::destroy([[maybe_unused]] WServer *server)
{
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
