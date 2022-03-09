/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wbackend.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wserver.h"
#include "winputdevice.h"
#include "wsignalconnector.h"

#include <QDebug>

extern "C" {
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_input_device.h>
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WBackendPrivate : public WObjectPrivate
{
public:
    WBackendPrivate(WBackend *qq, WOutputLayout *layout)
        : WObjectPrivate(qq)
        , layout(layout)
    {

    }

    inline wlr_backend *handle() const {
        return q_func()->nativeInterface<wlr_backend>();
    }

    // begin slot function
    void on_new_output(void *data);
    void on_new_input(void *data);
    void on_input_destroy(void *data);
    void on_output_destroy(void *data);
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WBackend)

    WServer *server = nullptr;
    WOutputLayout *layout = nullptr;
    wlr_renderer *renderer = nullptr;
    wlr_allocator *allocator = nullptr;

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

    WSignalConnector sc;
};

void WBackendPrivate::on_new_output(void *data)
{
    wlr_output *wlr_output = reinterpret_cast<struct wlr_output*>(data);

    /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
     * before we can use the output. The mode is a tuple of (width, height,
     * refresh rate), and each monitor supports only a specific set of modes. We
     * just pick the monitor's preferred mode, a more sophisticated compositor
     * would let the user configure it. */
    if (!wl_list_empty(&wlr_output->modes)) {
        struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
        wlr_output_set_mode(wlr_output, mode);
        wlr_output_enable(wlr_output, true);
        bool ok = wlr_output_commit(wlr_output);
        Q_ASSERT(ok);
    }

    auto handle = reinterpret_cast<WOutputHandle*>(wlr_output);
    auto output = new WOutput(handle, q_func());

    outputList << output;
    sc.connect(&wlr_output->events.destroy, this, &WBackendPrivate::on_output_destroy);

    layout->add(output);
}

void WBackendPrivate::on_new_input(void *data)
{
    auto device = reinterpret_cast<wlr_input_device*>(data);
    auto input_device = new WInputDevice(reinterpret_cast<WInputDeviceHandle*>(device));
    inputList << input_device;
    sc.connect(&device->events.destroy, this, &WBackendPrivate::on_input_destroy);

    Q_EMIT server->inputAdded(q_func(), input_device);
}

void WBackendPrivate::on_input_destroy(void *data)
{
    for (int i = 0; i < outputList.count(); ++i) {
        if (outputList.at(i)->handle() == data) {
            auto device = inputList.takeAt(i);
            Q_EMIT server->inputRemoved(q_func(), device);
            delete device;
            return;
        }
    }
}

void WBackendPrivate::on_output_destroy(void *data)
{
    for (int i = 0; i < outputList.count(); ++i) {
        if (outputList.at(i)->handle() == data) {
            auto device = outputList.takeAt(i);
            layout->remove(device);
            device->detach();
            delete device;
            return;
        }
    }
}

void WBackendPrivate::connect()
{
    sc.connect(&handle()->events.new_output,
               this, &WBackendPrivate::on_new_output);
    sc.connect(&handle()->events.new_input,
               this, &WBackendPrivate::on_new_input);
    sc.connect(&handle()->events.destroy,
               &sc, &WSignalConnector::invalidate);
}

WBackend::WBackend(WOutputLayout *layout)
    : WObject(*new WBackendPrivate(this, layout))
{

}

WRendererHandle *WBackend::renderer() const
{
    W_DC(WBackend);
    return reinterpret_cast<WRendererHandle*>(d->renderer);
}

WAllocatorHandle *WBackend::allocator() const
{
    W_DC(WBackend);
    return reinterpret_cast<WAllocatorHandle*>(d->allocator);
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

WServer *WBackend::server() const
{
    W_DC(WBackend);
    return d->server;
}

void WBackend::create(WServer *server)
{
    W_D(WBackend);

    if (!m_handle) {
        m_handle = wlr_backend_autocreate(server->nativeInterface<wl_display>());
    }

    d->renderer = wlr_renderer_autocreate(nativeInterface<wlr_backend>());
    d->allocator = wlr_allocator_autocreate(d->handle(), d->renderer);
    wlr_renderer_init_wl_display(d->renderer, server->nativeInterface<wl_display>());

    // free follow display
    wlr_compositor_create(server->nativeInterface<wl_display>(), d->renderer);

    d->server = server;
    d->connect();

    if (!wlr_backend_start(d->handle())) {
        qFatal("Start wlr backend falied");
    }
}

void WBackend::destroy(WServer *server)
{
    Q_UNUSED(server)
    W_D(WBackend);
    qDeleteAll(d->inputList);
    qDeleteAll(d->outputList);
    d->inputList.clear();
    d->outputList.clear();
    d->renderer = nullptr;
    d->allocator = nullptr;
    m_handle = nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
