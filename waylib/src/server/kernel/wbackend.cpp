// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbackend.h"
#include "woutput.h"
#include "wserver.h"
#include "winputdevice.h"
#include "wseat.h"
#include "wscoplistener.h"
#include "platformplugin/qwlrootsintegration.h"
#include "platformplugin/qwlrootscreen.h"
#include "private/wglobal_p.h"

#include <wlr_all.h>

#include <wayland-server-core.h>

#include <QDebug>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WBackendPrivate : public WObjectPrivate
{
public:
    WBackendPrivate(WBackend *qq)
        : WObjectPrivate(qq)
    {

    }

    inline wlr_backend *handle() const {
        return reinterpret_cast<wlr_backend*>(q_func()->m_handle);
    }

    // begin slot function
    void on_new_output(wlr_output *output);
    void on_new_input(wlr_input_device *device);
    void on_input_destroy(wlr_input_device *device);
    void on_output_destroy(wlr_output *output);
    void on_destroy();
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WBackend)

    QList<WOutput*> outputList;
    QList<WInputDevice*> inputList;

private:
    wlr_session *session = nullptr;
};

void WBackendPrivate::on_new_output(wlr_output *output)
{
    W_Q(WBackend);
    auto woutput = new WOutput(output, q);

    outputList << woutput;
    QWlrootsIntegration::instance()->addScreen(woutput);

    auto *listeners = woutput->listeners(q_ptr);
    listeners->add(&output->events.destroy, this,
        [this, output] (void *) {
        on_output_destroy(output);
    });

    Q_EMIT q->outputAdded(woutput);
}

void WBackendPrivate::on_new_input(wlr_input_device *device)
{
    W_Q(WBackend);
    auto winput_device = new WInputDevice(device);
    inputList << winput_device;
    auto *listeners = winput_device->listeners(q_ptr);
    listeners->add(&device->events.destroy, this,
        [this, device] (void *) {
        on_input_destroy(device);
    });

    Q_EMIT q->inputAdded(winput_device);
}

void WBackendPrivate::on_input_destroy(wlr_input_device *device)
{
    for (int i = 0; i < inputList.count(); ++i) {
        if (inputList.at(i)->handle() != device)
            continue;
        auto winput_device = inputList.takeAt(i);

        W_Q(WBackend);
        Q_EMIT q->inputRemoved(winput_device);
        // Safe to destroy the wrapper from inside its own destroy callback:
        // the listener closure is reference-counted and outlives the
        // emission, and ~WInputDevice clears the reverse mapping while the
        // native handle is still valid.
        delete winput_device;
        return;
    }
}

void WBackendPrivate::on_output_destroy(wlr_output *output)
{
    for (int i = 0; i < outputList.count(); ++i) {
        if (outputList.at(i)->handle() != output)
            continue;
        auto woutput = outputList.takeAt(i);

        W_Q(WBackend);
        Q_EMIT q->outputRemoved(woutput);
        QWlrootsIntegration::instance()->removeScreen(woutput);
        // Safe to destroy the wrapper from inside its own destroy callback
        // (see on_input_destroy).
        delete woutput;
        return;
    }
}

void WBackendPrivate::on_destroy()
{
    W_Q(WBackend);
    // The backend is being destroyed by wlroots itself (e.g. the primary
    // DRM backend was removed, which tears down the multi-backend).
    // wlr_backend_finish() asserts that every listener list is empty, so
    // detach ours before the native signal storage is freed, and drop the
    // dangling handle: handle()/hasDrm()/hasX11()/hasWayland() must not
    // dereference a backend that is going away. The wrapper objects for
    // outputs/inputs clean themselves up when the sub-backends destroy
    // their devices right after this emission.
    q_ptr->removeListeners(q_ptr);
    q->m_handle = nullptr;
    session = nullptr;
}

void WBackendPrivate::connect()
{
    W_Q(WBackend);
    q->listeners()->add(&handle()->events.new_output, this, &WBackendPrivate::on_new_output);
    q->listeners()->add(&handle()->events.new_input, this, &WBackendPrivate::on_new_input);
    q->listeners()->add(&handle()->events.destroy, this, &WBackendPrivate::on_destroy);
}

WBackend::WBackend()
    : WObject(*new WBackendPrivate(this))
{

}

wlr_backend *WBackend::handle() const
{
    return reinterpret_cast<wlr_backend*>(m_handle);
}

wlr_session *WBackend::session() const
{
    W_DC(WBackend);
    return d->session;
}

QList<WOutput*> WBackend::outputList() const
{
    W_DC(WBackend);
    return d->outputList;
}

QList<WInputDevice *> WBackend::inputDeviceList() const
{
    W_DC(WBackend);
    return d->inputList;
}

static bool hasBackend(wlr_backend *handle, bool (*isType)(struct wlr_backend *))
{
    // The backend may not exist yet (before start) or anymore (after
    // destroy / wlroots-side teardown): treat it as absent instead of
    // dereferencing a null handle.
    if (!handle)
        return false;
    if (isType(handle))
        return true;
    if (!wlr_backend_is_multi(handle))
        return false;

    struct BackendTypeChecker {
        bool (*isType)(struct wlr_backend *);
        bool exists = false;
    } checker { isType };

    wlr_multi_for_each_backend(handle, [] (struct wlr_backend *backend, void *data) {
        auto *checker = static_cast<BackendTypeChecker*>(data);
        if (checker->isType(backend))
            checker->exists = true;
    }, &checker);

    return checker.exists;
}

bool WBackend::hasDrm() const
{
    return hasBackend(handle(), &wlr_backend_is_drm);
}

bool WBackend::hasX11() const
{
    return hasBackend(handle(), &wlr_backend_is_x11);
}

bool WBackend::hasWayland() const
{
    return hasBackend(handle(), &wlr_backend_is_wl);
}

bool WBackend::isSessionActive() const
{
    W_DC(WBackend);
    return d->session && d->session->active;
}

void WBackend::create(WServer *server)
{
    W_D(WBackend);

    if (!m_handle) {
        wlr_session *session = nullptr;
        m_handle = wlr_backend_autocreate(wl_display_get_event_loop(server->handle()), &session);
        Q_ASSERT(m_handle);
        d->session = session;
        Q_EMIT created();
    }

    d->connect();
}

void WBackend::destroy([[maybe_unused]] WServer *server)
{
    W_D(WBackend);

    QList<WInputDevice*> inputList;
    inputList.swap(d->inputList);
    QList<WOutput*> outputList;
    outputList.swap(d->outputList);
    for (auto *device : std::as_const(inputList))
        delete device;
    for (auto *output : std::as_const(outputList))
        delete output;

    removeListeners(this);
    auto *backend = reinterpret_cast<wlr_backend*>(m_handle);
    m_handle = nullptr;
    d->session = nullptr;
    if (backend)
        wlr_backend_destroy(backend);
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
