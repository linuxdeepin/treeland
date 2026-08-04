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
#include <qwsession.h>

#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>

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
    void on_input_destroy(WInputDevice *data);
    void on_output_destroy(WOutput *output);
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WBackend)

    QList<WOutput*> outputList;
    QList<WInputDevice*> inputList;

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
    auto woutput = new WOutput(output, q);

    outputList << woutput;
    QWlrootsIntegration::instance()->addScreen(woutput);

    QObject::connect(woutput, &WWrapObject::aboutToBeInvalidated, q, [this, woutput] {
        on_output_destroy(woutput);
    });

    Q_EMIT q->outputAdded(woutput);
}

void WBackendPrivate::on_new_input(wlr_input_device *device)
{
    W_Q(WBackend);
    auto winput_device = new WInputDevice(device);
    inputList << winput_device;
    QObject::connect(winput_device, &WWrapObject::aboutToBeInvalidated, q, [this, winput_device] {
        on_input_destroy(winput_device);
    });

    Q_EMIT q->inputAdded(winput_device);
}

void WBackendPrivate::on_input_destroy(WInputDevice *data)
{
    for (int i = 0; i < inputList.count(); ++i) {
        if (inputList.at(i) == data) {
            auto device = inputList.takeAt(i);

            W_Q(WBackend);
            Q_EMIT q->inputRemoved(device);
            device->safeDeleteLater();
            return;
        }
    }
}

void WBackendPrivate::on_output_destroy(WOutput *output)
{
    for (int i = 0; i < outputList.count(); ++i) {
        if (outputList.at(i) == output) {
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

wlr_backend *WBackend::handle() const
{
    return nativeInterface<qw_backend>()->handle();
}

wlr_session *WBackend::session() const
{
    W_DC(WBackend);
    return d->session ? d->session->handle() : nullptr;
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

static bool backendHasType(wlr_backend *handle, bool (*isType)(wlr_backend *))
{
    if (isType(handle))
        return true;
    if (wlr_backend_is_multi(handle)) {
        struct Ctx { bool (*isType)(wlr_backend *); bool exists; };
        Ctx ctx{isType, false};
        wlr_multi_for_each_backend(handle, [] (wlr_backend *backend, void *data) {
            auto *ctx = static_cast<Ctx*>(data);
            if (ctx->isType(backend))
                ctx->exists = true;
        }, &ctx);

        return ctx.exists;
    }

    return false;
}

bool WBackend::hasDrm() const
{
    return backendHasType(handle(), wlr_backend_is_drm);
}

bool WBackend::hasX11() const
{
#ifdef WLR_HAVE_X11_BACKEND
    return backendHasType(handle(), wlr_backend_is_x11);
#else
    return false;
#endif
}

bool WBackend::hasWayland() const
{
    return backendHasType(handle(), wlr_backend_is_wl);
}

bool WBackend::isSessionActive() const
{
    W_D(const WBackend);
    return d->session && d->session->handle()->active;
}

void WBackend::activateSession()
{
    W_D(WBackend);
    if (d->session) {
        struct wlr_session *session = d->session->handle();
        session->active = true;
        wl_signal_emit_mutable(&session->events.active, nullptr);
    }
}

void WBackend::deactivateSession()
{
    W_D(WBackend);
    if (d->session) {
        struct wlr_session *session = d->session->handle();
        session->active = false;
        wl_signal_emit_mutable(&session->events.active, nullptr);
    }
}

void WBackend::create(WServer *server)
{
    W_D(WBackend);

    if (!m_handle) {
        wlr_session *session = nullptr;
        m_handle = qw_backend::autocreate(wl_display_get_event_loop(server->handle()), &session);
        Q_ASSERT(m_handle);
        d->session = qw_session::from(session);
        Q_EMIT created();
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
