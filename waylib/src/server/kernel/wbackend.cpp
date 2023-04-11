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

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwrenderer.h>
#include <qwallocator.h>
#include <qwcompositor.h>
#include <qwoutput.h>
#include <qwinputdevice.h>

#include <QDebug>

extern "C" {
#include <wlr/backend.h>
#include <wlr/backend/interface.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_input_device.h>
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#undef static
#include <wlr/render/gles2.h>
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QQuickWindow>
#include <xf86drm.h>
#include <fcntl.h>
#include <unistd.h>
#endif

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WBackendPrivate : public WObjectPrivate
{
public:
    WBackendPrivate(WBackend *qq, WOutputLayout *layout)
        : WObjectPrivate(qq)
        , layout(layout)
    {

    }

    inline QWBackend *handle() const {
        return q_func()->nativeInterface<QWBackend>();
    }

    // begin slot function
    void on_new_output(QWOutput *output);
    void on_new_input(QWInputDevice *device);
    void on_input_destroy(void *data);
    void on_output_destroy(void *data);
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WBackend)

    WServer *server = nullptr;
    WOutputLayout *layout = nullptr;
    QWRenderer *renderer = nullptr;
    QWAllocator *allocator = nullptr;

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
    output->initRender(allocator, renderer);

    /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
     * before we can use the output. The mode is a tuple of (width, height,
     * refresh rate), and each monitor supports only a specific set of modes. We
     * just pick the monitor's preferred mode, a more sophisticated compositor
     * would let the user configure it. */
    if (!wl_list_empty(&output->handle()->modes)) {
        struct wlr_output_mode *mode = output->preferredMode();
        output->setMode(mode);
        output->enable(true);
        bool ok = output->commit();
        Q_ASSERT(ok);
    }

    auto handle = reinterpret_cast<WOutputHandle*>(output);
    auto woutput = new WOutput(handle, q_func());

    outputList << woutput;
    QObject::connect(output, &QObject::destroyed, server, [this] (QObject *data) {
        on_output_destroy(data);
    });

    layout->add(woutput);
}

void WBackendPrivate::on_new_input(QWInputDevice *device)
{
    auto input_device = new WInputDevice(reinterpret_cast<WInputDeviceHandle*>(device));
    inputList << input_device;
    QObject::connect(device, &QObject::destroyed, server, [this] (QObject *data) {
        on_input_destroy(data);
    });

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
    QObject::connect(handle(), &QWBackend::newOutput, server, [this] (QWOutput *output) {
        on_new_output(output);
    });
    QObject::connect(handle(), &QWBackend::newInput, server, [this] (QWInputDevice *device) {
        on_new_input(device);
    });
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// Copy from "wlr_renderer.c" of wlroots
static int open_drm_render_node(void) {
	uint32_t flags = 0;
	int devices_len = drmGetDevices2(flags, NULL, 0);
	if (devices_len < 0) {
		qFatal("drmGetDevices2 failed: %s", strerror(-devices_len));
		return -1;
	}
	drmDevice **devices = reinterpret_cast<drmDevice**>(calloc(devices_len, sizeof(drmDevice *)));
	if (devices == NULL) {
		qFatal("Allocation failed: %s", strerror(errno));
		return -1;
	}
	devices_len = drmGetDevices2(flags, devices, devices_len);
	if (devices_len < 0) {
		free(devices);
		qFatal("drmGetDevices2 failed: %s", strerror(-devices_len));
		return -1;
	}

	int fd = -1;
	for (int i = 0; i < devices_len; i++) {
		drmDevice *dev = devices[i];
		if (dev->available_nodes & (1 << DRM_NODE_RENDER)) {
			const char *name = dev->nodes[DRM_NODE_RENDER];
			qDebug("Opening DRM render node '%s'", name);
			fd = open(name, O_RDWR | O_CLOEXEC);
			if (fd < 0) {
				qFatal("Failed to open '%s': %s", name, strerror(errno));
				goto out;
			}
			break;
		}
	}
	if (fd < 0) {
		qFatal("Failed to find any DRM render node");
	}

out:
	for (int i = 0; i < devices_len; i++) {
		drmFreeDevice(&devices[i]);
	}
	free(devices);

	return fd;
}
#endif

void WBackend::create(WServer *server)
{
    W_D(WBackend);

    if (!m_handle) {
        m_handle = QWBackend::autoCreate(server->nativeInterface<QWDisplay>());
    }

    auto backend = nativeInterface<QWBackend>();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    int drm_fd = wlr_backend_get_drm_fd(backend->handle());

    auto backend_get_buffer_caps = [] (struct wlr_backend *backend) -> uint32_t {
        if (!backend->impl->get_buffer_caps) {
            return 0;
        }

        return backend->impl->get_buffer_caps(backend);
    };

    uint32_t backend_caps = backend_get_buffer_caps(backend->handle());
    int render_drm_fd = -1;
    if (drm_fd < 0 && (backend_caps & WLR_BUFFER_CAP_DMABUF) != 0) {
        render_drm_fd = open_drm_render_node();
        drm_fd = render_drm_fd;
    }

    wlr_renderer *renderer_handle = nullptr;

    switch (QQuickWindow::graphicsApi()) {
    case QSGRendererInterface::OpenGL:
        renderer_handle = wlr_gles2_renderer_create_with_drm_fd(drm_fd);
        break;
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        renderer_handle = wlr_vk_renderer_create_with_drm_fd(drm_fd);
        break;
    }
#endif
    case QSGRendererInterface::Software:
        renderer_handle = wlr_pixman_renderer_create();
        break;
    default:
        qFatal("Not supported graphics api: %s", qPrintable(QQuickWindow::sceneGraphBackend()));
        break;
    }

    if (renderer_handle) {
        d->renderer = QWRenderer::from(renderer_handle);
    }

    if (render_drm_fd >= 0) {
        close(render_drm_fd);
    }

#else
    d->renderer = QWRenderer::autoCreate(backend);
#endif

    if (!d->renderer) {
        qFatal("Failed to create renderer");
    }

    d->allocator = QWAllocator::autoCreate(backend, d->renderer);
    d->renderer->initWlDisplay(server->nativeInterface<QWDisplay>());

    // free follow display
    Q_UNUSED(QWCompositor::create(server->nativeInterface<QWDisplay>(), d->renderer));

    d->server = server;
    d->connect();

    if (!backend->start()) {
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
