// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputhelper.h"
#include "woutput.h"
#include "wbackend.h"
#include "wtools.h"
#include "platformplugin/types.h"

#include <qwbackend.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwbuffer.h>
#include <qwtexture.h>
#include <platformplugin/qwlrootswindow.h>
#include <platformplugin/qwlrootsintegration.h>
#include <platformplugin/qwlrootscreen.h>

#include <QWindow>
#include <QQuickWindow>
#ifndef QT_NO_OPENGL
#include <QOpenGLContext>
#endif

extern "C" {
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/gles2.h>
#undef static
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
#include <wlr/types/wlr_output_damage.h>
#endif
#include <wlr/render/pixman.h>
#include <wlr/render/egl.h>
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
#include <wlr/util/region.h>
#include <wlr/types/wlr_output.h>
#include <wlr/backend.h>
#include <wlr/backend/interface.h>
}

#include <xf86drm.h>
#include <fcntl.h>
#include <unistd.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputHelperPrivate : public WObjectPrivate
{
public:
    WOutputHelperPrivate(WOutput *output, WOutputHelper *qq)
        : WObjectPrivate(qq)
        , output(output)
        , outputWindow(new QW::Window)
    {
        outputWindow->QObject::setParent(qq);
        outputWindow->setScreen(QWlrootsIntegration::instance()->getScreenFrom(output)->screen());
        outputWindow->create();

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
        damage = new WOutputDamage(q);
#endif

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
        sc.connect(&damage->nativeInterface<wlr_output_damage>()->events.frame,
                   this, &WOutputWindowPrivate::on_damage_frame);
        sc.connect(&damage->nativeInterface<wlr_output_damage>()->events.destroy,
                   &sc, &WSignalConnector::invalidate);
#else
        QObject::connect(qwoutput(), &QWOutput::frame, qq, [this] {
            on_frame();
        });
        QObject::connect(qwoutput(), &QWOutput::damage, qq, [this] {
            on_damage();
        });
        QObject::connect(output, &WOutput::modeChanged, qq, [this] {
            resetRenderBuffer();
        });
#endif
        // In call the connect for 'frame' signal before, maybe the wlr_output object is already \
        // emit the signal, so we should suppose the renderable is true in order that ensure can \
        // render on the next time
        renderable = true;
    }

    inline QWOutput *qwoutput() const {
        return output->handle();
    }

    inline QWRenderer *renderer() const {
        return output->renderer();
    }

    inline QWSwapchain *swapchain() const {
        return output->swapchain();
    }

    inline QWlrootsOutputWindow *qpaWindow() const {
        return static_cast<QWlrootsOutputWindow*>(outputWindow->handle());
    }

    void setRenderable(bool newValue);
    void setContentIsDirty(bool newValue);

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    void on_damage_frame(void *);
#else
    void on_frame();
    void on_damage();
#endif
    void resetRenderBuffer();

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    inline bool attachRender(pixman_region32_t *damage, bool *needsFrame = nullptr) {
        if (bufferIsAttached()) {
            // Don't reuse the state, needs ensuse the damage data is valid
            qwoutput()->rollback();
        }

        bool needs_frame;
        if (!wlr_output_damage_attach_render(this->damage->nativeInterface<wlr_output_damage>(),
                                             &needs_frame, damage)) {
            return false;
        }

        if (needsFrame)
            *needsFrame = needs_frame;

        attachedBuffer = true;
        return true;
    }
#endif

    QWBuffer *ensureRenderBuffer();

    inline bool makeCurrent(QOpenGLContext *context) {
        if (!ensureRenderBuffer())
            return false;

        if (context) {
            return context->makeCurrent(outputWindow);
        } else {
            return qpaWindow()->attachRenderer();
        }
    }

    inline void doneCurrent(QOpenGLContext *context) {
        if (context) {
            context->doneCurrent();
        } else {
            qpaWindow()->detachRenderer();
        }
    }

    inline void update() {
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
        damage->addWhole();
#else
        setContentIsDirty(true);
#endif
    }

    W_DECLARE_PUBLIC(WOutputHelper)
    WOutput *output;
    QWindow *outputWindow;

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    WOutputDamage *damage = nullptr;
#endif
    // for software renderer
    QImage paintDevice;
    std::unique_ptr<QWTexture> renderTexture;
    QQuickRenderTarget renderTarget;

    bool renderable = false;
#ifdef WAYLIB_DISABLE_OUTPUT_DAMAGE
    bool contentIsDirty = false;
#endif
};

void WOutputHelperPrivate::setRenderable(bool newValue)
{
    if (renderable == newValue)
        return;
    renderable = newValue;
    Q_EMIT q_func()->renderableChanged();
}

void WOutputHelperPrivate::setContentIsDirty(bool newValue)
{
    if (contentIsDirty == newValue)
        return;
    contentIsDirty = newValue;
    Q_EMIT q_func()->contentIsDirtyChanged();
}

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
void WOutputHelperPrivate::on_damage_frame(void *)
{
    renderable = true;
    Q_EMIT q_func()->requestRender();
}
#else
void WOutputHelperPrivate::on_frame()
{
    setRenderable(true);
    Q_EMIT q_func()->requestRender();
}

void WOutputHelperPrivate::on_damage()
{
    setContentIsDirty(true);
    Q_EMIT q_func()->damaged();
}
#endif

void WOutputHelperPrivate::resetRenderBuffer()
{
    renderTexture.reset();
    renderTarget = {};
    qpaWindow()->setBuffer(nullptr);
}

QWBuffer *WOutputHelperPrivate::ensureRenderBuffer()
{
    if (auto buffer = qpaWindow()->buffer())
        return buffer;

    bool ok = wlr_output_configure_primary_swapchain(qwoutput()->handle(),
                                                     &qwoutput()->handle()->pending,
                                                     &qwoutput()->handle()->swapchain);
    if (!ok)
        return nullptr;
    QWBuffer *newBuffer = swapchain()->acquire(nullptr);
    qpaWindow()->setBuffer(newBuffer);

    return newBuffer;
}

WOutputHelper::WOutputHelper(WOutput *output, QObject *parent)
    : QObject(parent)
    , WObject(*new WOutputHelperPrivate(output, this))
{

}

WOutput *WOutputHelper::output() const
{
    W_DC(WOutputHelper);
    return d->output;
}

QWindow *WOutputHelper::outputWindow() const
{
    W_DC(WOutputHelper);
    return d->outputWindow;
}

QQuickRenderTarget WOutputHelper::makeRenderTarget()
{
    W_D(WOutputHelper);

    if (!d->renderTarget.isNull())
        return d->renderTarget;

    QWBuffer *buffer = d->ensureRenderBuffer();
    if (!buffer)
        return {};

    d->renderTexture.reset(QWTexture::fromBuffer(d->renderer(), buffer));

    if (wlr_renderer_is_pixman(d->renderer()->handle())) {
        pixman_image_t *image = wlr_pixman_texture_get_image(d->renderTexture->handle());
        void *data = pixman_image_get_data(image);
        if (Q_UNLIKELY(d->paintDevice.constBits() != data))
            d->paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!d->paintDevice.isNull());
        d->paintDevice.setDevicePixelRatio(d->qwoutput()->handle()->pending.scale);
        return QQuickRenderTarget::fromPaintDevice(&d->paintDevice);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_renderer_is_vk(d->renderer()->handle())) {
        wlr_vk_image_attribs attribs;
        wlr_vk_texture_get_image_attribs(d->renderTexture->handle(), &attribs);
        return QQuickRenderTarget::fromVulkanImage(attribs.image, attribs.layout, attribs.format, d->output->size());
    }
#endif
    else if (wlr_renderer_is_gles2(d->renderer()->handle())) {
        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(d->renderTexture->handle(), &attribs);

        auto rt = QQuickRenderTarget::fromOpenGLTexture(attribs.tex, d->output->size());
        rt.setMirrorVertically(true);
        return rt;
    }

    return {};
}

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

QWRenderer *WOutputHelper::createRenderer(QWBackend *backend)
{
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
    auto api = QQuickWindow::graphicsApi();
    if (QQuickWindow::sceneGraphBackend() == QStringLiteral("software"))
        api = QSGRendererInterface::Software;

    switch (api) {
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

    QWRenderer *renderer = nullptr;
    if (renderer_handle) {
        renderer = QWRenderer::from(renderer_handle);
    }

    if (render_drm_fd >= 0) {
        close(render_drm_fd);
    }

    return renderer;
}

bool WOutputHelper::renderable() const
{
    W_DC(WOutputHelper);
    return d->renderable;
}

bool WOutputHelper::contentIsDirty() const
{
    W_DC(WOutputHelper);
    return d->contentIsDirty;
}

bool WOutputHelper::makeCurrent(QOpenGLContext *context)
{
    W_D(WOutputHelper);
    return d->makeCurrent(context);
}

void WOutputHelper::doneCurrent(QOpenGLContext *context)
{
    W_D(WOutputHelper);
    d->doneCurrent(context);
}

void WOutputHelper::resetState()
{
    W_D(WOutputHelper);
    d->setContentIsDirty(false);
    d->setRenderable(false);
}

void WOutputHelper::update()
{
    W_D(WOutputHelper);
    d->update();
}

WAYLIB_SERVER_END_NAMESPACE
