// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputhelper.h"
#include "woutput.h"
#include "wbackend.h"
#include "wtools.h"
#include "platformplugin/types.h"
#include "private/wqmlhelper_p.h"

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
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>

extern "C" {
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/gles2.h>
#undef static
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

struct BufferData {
    BufferData() {

    }

    ~BufferData() {
        resetWindowRenderTarget();
    }

    QWBuffer *buffer = nullptr;
    // for software renderer
    WImageRenderTarget paintDevice;
    QQuickRenderTarget renderTarget;
    QQuickWindowRenderTarget windowRenderTarget;

    inline void resetWindowRenderTarget() {
        if (windowRenderTarget.owns) {
            delete windowRenderTarget.renderTarget;
            delete windowRenderTarget.rpDesc;
            delete windowRenderTarget.texture;
            delete windowRenderTarget.renderBuffer;
            delete windowRenderTarget.depthStencil;
            delete windowRenderTarget.paintDevice;
        }

        windowRenderTarget.renderTarget = nullptr;
        windowRenderTarget.rpDesc = nullptr;
        windowRenderTarget.texture = nullptr;
        windowRenderTarget.renderBuffer = nullptr;
        windowRenderTarget.depthStencil = nullptr;
        windowRenderTarget.paintDevice = nullptr;
        windowRenderTarget.owns = false;
    }
};

class WOutputHelperPrivate : public WObjectPrivate
{
public:
    WOutputHelperPrivate(WOutput *output, WOutputHelper *qq)
        : WObjectPrivate(qq)
        , output(output)
        , outputWindow(new QW::Window)
        , renderable(false)
        , contentIsDirty(false)
        , needsFrame(false)
    {
        outputWindow->QObject::setParent(qq);
        outputWindow->setScreen(QWlrootsIntegration::instance()->getScreenFrom(output)->screen());
        outputWindow->create();

        QObject::connect(qwoutput(), &QWOutput::frame, qq, [this] {
            on_frame();
        });
        QObject::connect(qwoutput(), &QWOutput::needsFrame, qq, [this] {
            setNeedsFrame(true);
            qwoutput()->QWOutput::scheduleFrame();
        });
        QObject::connect(qwoutput(), &QWOutput::damage, qq, [this] {
            on_damage();
        });
        QObject::connect(output, &WOutput::modeChanged, qq, [this] {
            resetRenderBuffer();
        }, Qt::QueuedConnection); // reset buffer on later, because it's rendering

        // In call the connect for 'frame' signal before, maybe the wlr_output object is already \
        // emit the signal, so we should suppose the renderable is true in order that ensure can \
        // render on the next time
        renderable = true;
    }
    ~WOutputHelperPrivate() {
        lastBuffer = nullptr;
        qDeleteAll(buffers);
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
    void setNeedsFrame(bool newNeedsFrame);

    void on_frame();
    void on_damage();

    void resetRenderBuffer();
    QWBuffer *acquireBuffer(int *bufferAge);
    void onBufferDestroy(QWBuffer *buffer);
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data);

    inline bool makeCurrent(QWBuffer *buffer, QOpenGLContext *context) {
        qpaWindow()->setBuffer(buffer);
        bool ok = false;
#ifndef QT_NO_OPENGL
        if (context) {
            ok = context->makeCurrent(outputWindow);
        } else
#endif
        {
            ok = qpaWindow()->attachRenderer();
        }

        if (!ok)
            buffer->unlock();

        return ok;
    }

    inline void doneCurrent(QOpenGLContext *context) {
#ifndef QT_NO_OPENGL
        if (context) {
            context->doneCurrent();
        } else
#endif
        {
            qpaWindow()->detachRenderer();
        }
    }

    inline void update() {
        setContentIsDirty(true);
    }

    W_DECLARE_PUBLIC(WOutputHelper)
    WOutput *output;
    QWindow *outputWindow;

    QList<BufferData*> buffers;
    BufferData *lastBuffer = nullptr;

    uint renderable:1;
    uint contentIsDirty:1;
    uint needsFrame:1;
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

void WOutputHelperPrivate::setNeedsFrame(bool newNeedsFrame)
{
    if (needsFrame == newNeedsFrame)
        return;
    needsFrame = newNeedsFrame;
    Q_EMIT q_func()->needsFrameChanged();
}

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

void WOutputHelperPrivate::resetRenderBuffer()
{
    qpaWindow()->setBuffer(nullptr);
    qDeleteAll(buffers);
    lastBuffer = nullptr;
    buffers.clear();
}

QWBuffer *WOutputHelperPrivate::acquireBuffer(int *bufferAge)
{
    bool ok = wlr_output_configure_primary_swapchain(qwoutput()->handle(),
                                                     &qwoutput()->handle()->pending,
                                                     &qwoutput()->handle()->swapchain);
    if (!ok)
        return nullptr;
    QWBuffer *newBuffer = swapchain()->acquire(bufferAge);
    return newBuffer;
}

void WOutputHelperPrivate::onBufferDestroy(QWBuffer *buffer)
{
    for (int i = 0; i < buffers.count(); ++i) {
        auto data = buffers[i];
        if (data->buffer == buffer) {
            if (lastBuffer == data)
                lastBuffer = nullptr;
            buffers.removeAt(i);
            break;
        }
    }
}

// Copy from qquickrendertarget.cpp
static bool createRhiRenderTarget(const QRhiColorAttachment &colorAttachment,
                                  const QSize &pixelSize,
                                  int sampleCount,
                                  QRhi *rhi,
                                  QQuickWindowRenderTarget &dst)
{
    std::unique_ptr<QRhiRenderBuffer> depthStencil(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, sampleCount));
    if (!depthStencil->create()) {
        qWarning("Failed to build depth-stencil buffer for QQuickRenderTarget");
        return false;
    }

    QRhiTextureRenderTargetDescription rtDesc(colorAttachment);
    rtDesc.setDepthStencilBuffer(depthStencil.get());
    std::unique_ptr<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());

    if (!rt->create()) {
        qWarning("Failed to build texture render target for QQuickRenderTarget");
        return false;
    }

    dst.renderTarget = rt.release();
    dst.rpDesc = rp.release();
    dst.depthStencil = depthStencil.release();
    dst.owns = true; // ownership of the native resource itself is not transferred but the QRhi objects are on us now

    return true;
}

bool createRhiRenderTarget(QRhi *rhi, const QQuickRenderTarget &source, QQuickWindowRenderTarget &dst)
{
    auto rtd = QQuickRenderTargetPrivate::get(&source);

    switch (rtd->type) {
    case QQuickRenderTargetPrivate::Type::NativeTexture: {
        const auto format = rtd->u.nativeTexture.rhiFormat == QRhiTexture::UnknownFormat ? QRhiTexture::RGBA8
                                                                                         : QRhiTexture::Format(rtd->u.nativeTexture.rhiFormat);
        const auto flags = QRhiTexture::RenderTarget | QRhiTexture::Flags(rtd->u.nativeTexture.rhiFlags);
        std::unique_ptr<QRhiTexture> texture(rhi->newTexture(format, rtd->pixelSize, rtd->sampleCount, flags));
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layout }))
#else
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layoutOrState }))
#endif
            return false;
        QRhiColorAttachment att(texture.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst))
            return false;
        dst.texture = texture.release();
        return true;
    }
    case QQuickRenderTargetPrivate::Type::NativeRenderbuffer: {
        std::unique_ptr<QRhiRenderBuffer> renderbuffer(rhi->newRenderBuffer(QRhiRenderBuffer::Color, rtd->pixelSize, rtd->sampleCount));
        if (!renderbuffer->createFrom({ rtd->u.nativeRenderbufferObject })) {
            qWarning("Failed to build wrapper renderbuffer for QQuickRenderTarget");
            return false;
        }
        QRhiColorAttachment att(renderbuffer.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst))
            return false;
        dst.renderBuffer = renderbuffer.release();
        return true;
    }

    default:
        break;
    }

    return false;
}
// Copy end

bool WOutputHelperPrivate::ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data)
{
    data->resetWindowRenderTarget();
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    auto rhi = QQuickRenderControlPrivate::get(rc)->rhi;
#else
    auto rhi = rc->rhi();
#endif
    auto tmp = data->renderTarget;
    bool ok = createRhiRenderTarget(rhi, tmp, data->windowRenderTarget);
    if (!ok)
        return false;
    data->renderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->windowRenderTarget.renderTarget);
    data->renderTarget.setDevicePixelRatio(tmp.devicePixelRatio());
    data->renderTarget.setMirrorVertically(tmp.mirrorVertically());

    return true;
}

WOutputHelper::WOutputHelper(WOutput *output, QObject *parent)
    : QObject(parent)
    , WObject(*new WOutputHelperPrivate(output, this))
{

}

QSGRendererInterface::GraphicsApi WOutputHelper::getGraphicsApi(QQuickRenderControl *rc)
{
    auto d = QQuickRenderControlPrivate::get(rc);
    return d->sg->rendererInterface(d->rc)->graphicsApi();
}

QSGRendererInterface::GraphicsApi WOutputHelper::getGraphicsApi()
{
    auto getApi = [] () {
        // Only for get GraphicsApi
        QQuickRenderControl rc;
        return getGraphicsApi(&rc);
    };

    static auto api = getApi();
    return api;
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

std::pair<QWBuffer*, QQuickRenderTarget> WOutputHelper::acquireRenderTarget(QQuickRenderControl *rc, int *bufferAge)
{
    W_D(WOutputHelper);

    QWBuffer *buffer = d->acquireBuffer(bufferAge);
    if (!buffer)
        return {};

    for (int i = 0; i < d->buffers.count(); ++i) {
        auto data = d->buffers[i];
        if (data->buffer == buffer) {
            d->lastBuffer = data;
            return {buffer, data->renderTarget};
        }
    }

    std::unique_ptr<BufferData> bufferData(new BufferData);

    bufferData->buffer = buffer;
    auto texture = QWTexture::fromBuffer(d->renderer(), buffer);

    QQuickRenderTarget rt;

    if (wlr_renderer_is_pixman(d->renderer()->handle())) {
        pixman_image_t *image = wlr_pixman_texture_get_image(texture->handle());
        void *data = pixman_image_get_data(image);
        if (bufferData->paintDevice.constBits() != data)
            bufferData->paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!bufferData->paintDevice.isNull());
        rt = QQuickRenderTarget::fromPaintDevice(&bufferData->paintDevice);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_renderer_is_vk(d->renderer()->handle())) {
        wlr_vk_image_attribs attribs;
        wlr_vk_texture_get_image_attribs(texture->handle(), &attribs);
        rt = QQuickRenderTarget::fromVulkanImage(attribs.image, attribs.layout, attribs.format, d->output->size());
    }
#endif
    else if (wlr_renderer_is_gles2(d->renderer()->handle())) {
        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(texture->handle(), &attribs);

        rt = QQuickRenderTarget::fromOpenGLTexture(attribs.tex, d->output->size());
        rt.setMirrorVertically(true);
    }

    delete texture;

    bufferData->renderTarget = rt;

    if (QSGRendererInterface::isApiRhiBased(getGraphicsApi(rc))) {
        if (!rt.isNull()) {
            // Force convert to Rhi render target
            if (!d->ensureRhiRenderTarget(rc, bufferData.get()))
                bufferData->renderTarget = {};
        }

        if (bufferData->renderTarget.isNull()) {
            buffer->unlock();
            return {};
        }
    }

    connect(buffer, SIGNAL(beforeDestroy(QWBuffer*)),
            this, SLOT(onBufferDestroy(QWBuffer*)), Qt::UniqueConnection);

    d->buffers.append(bufferData.release());
    d->lastBuffer = d->buffers.last();

    return {buffer, d->buffers.last()->renderTarget};
}

std::pair<QWBuffer *, QQuickRenderTarget> WOutputHelper::lastRenderTarget()
{
    W_DC(WOutputHelper);
    if (!d->lastBuffer)
        return {nullptr, {}};

    return {d->lastBuffer->buffer, d->lastBuffer->renderTarget};
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
    auto api = getGraphicsApi();

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

bool WOutputHelper::needsFrame() const
{
    W_DC(WOutputHelper);
    return d->needsFrame;
}

bool WOutputHelper::makeCurrent(QWBuffer *buffer, QOpenGLContext *context)
{
    W_D(WOutputHelper);
    return d->makeCurrent(buffer, context);
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
    d->setNeedsFrame(false);
}

void WOutputHelper::update()
{
    W_D(WOutputHelper);
    d->update();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputhelper.cpp"
