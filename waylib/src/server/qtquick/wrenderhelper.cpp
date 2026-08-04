// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderhelper.h"
#include "wtools.h"
#include "wayliblogging.h"
#include "private/wqmlhelper_p.h"
#include "private/wglobal_p.h"
#include "wimagebuffer.h"

#include <QSGTexture>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qsgplaintexture_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qsgsoftwarepixmaptexture_p.h>
#include <private/qsgrhisupport_p.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/egl.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
}
#include <EGL/eglext.h>
#include <drm_fourcc.h>
#include <dlfcn.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN RhiRenderEntry {
    const QRhiRenderTarget *renderTarget;
    const QRhiTexture *texture;
    wlr_buffer *buffer;
};

Q_GLOBAL_STATIC(QVector<RhiRenderEntry>, s_rhiRenderBuffers)

struct Q_DECL_HIDDEN BufferData {
    BufferData() {

    }

    ~BufferData() {
        resetWindowRenderTarget();
    }

    wlr_buffer *buffer = nullptr;
    WNativeListener destroyListener;
    // for software renderer
    WImageRenderTarget paintDevice;
    QQuickRenderTarget renderTarget;
    QQuickWindowRenderTarget windowRenderTarget;

    inline void resetWindowRenderTarget() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        {
            auto it = s_rhiRenderBuffers->begin();
            while (it != s_rhiRenderBuffers->end()) {
                if (windowRenderTarget.rt.renderTarget == it->renderTarget) {
                    it = s_rhiRenderBuffers->erase(it);
                    break;
                }
                ++it;
            }
        }

        if (windowRenderTarget.rt.owns)
            delete windowRenderTarget.rt.renderTarget;

        delete windowRenderTarget.res.texture;
        delete windowRenderTarget.res.renderBuffer;
        delete windowRenderTarget.res.rpDesc;

        windowRenderTarget.rt = {};
        windowRenderTarget.res = {};
        { // windowRenderTarget.implicitBuffers.reset(rhi);
            delete windowRenderTarget.implicitBuffers.depthStencil;
            delete windowRenderTarget.implicitBuffers.depthStencilTexture;
            delete windowRenderTarget.implicitBuffers.multisampleTexture;
            windowRenderTarget.implicitBuffers = {};
        }

        if (windowRenderTarget.sw.owns)
            delete windowRenderTarget.sw.paintDevice;

        windowRenderTarget.sw = {};
#else
        {
            auto it = s_rhiRenderBuffers->begin();
            while (it != s_rhiRenderBuffers->end()) {
                if (windowRenderTarget.renderTarget == it->renderTarget) {
                    it = s_rhiRenderBuffers->erase(it);
                    break;
                }
                ++it;
            }
        }

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
#endif
    }
};

// Copy from qquickrendertarget.cpp
static bool createRhiRenderTarget(const QRhiColorAttachment &colorAttachment,
                                  const QSize &pixelSize,
                                  int sampleCount,
                                  QRhi *rhi,
                                  QQuickWindowRenderTarget &dst)
{
    std::unique_ptr<QRhiRenderBuffer> depthStencil(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, sampleCount));
    if (!depthStencil->create()) {
        qCWarning(lcWlRenderHelper, "Failed to build depth-stencil buffer for QQuickRenderTarget");
        return false;
    }

    QRhiTextureRenderTargetDescription rtDesc(colorAttachment);
    rtDesc.setDepthStencilBuffer(depthStencil.get());
    std::unique_ptr<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());

    if (!rt->create()) {
        qCWarning(lcWlRenderHelper, "Failed to build texture render target for QQuickRenderTarget");
        return false;
    }

    rt->setName(QByteArrayLiteral("WaylibTextureRenderTarget"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    dst.rt.renderTarget = rt.release();
    dst.res.rpDesc = rp.release();
    dst.implicitBuffers.depthStencil = depthStencil.release();
    dst.rt.owns = true; // ownership of the native resource itself is not transferred but the QRhi objects are on us now
#else
    dst.renderTarget = rt.release();
    dst.rpDesc = rp.release();
    dst.depthStencil = depthStencil.release();
    dst.owns = true; // ownership of the native resource itself is not transferred but the QRhi objects are on us now
#endif
    return true;
}

bool createRhiRenderTarget(QRhi *rhi, const QQuickRenderTarget &source, QQuickWindowRenderTarget &dst)
{
    auto rtd = QQuickRenderTargetPrivate::get(&source);

    switch (rtd->type) {
    case QQuickRenderTargetPrivate::Type::NativeTexture: {
        const auto format = rtd->u.nativeTexture.rhiFormat == QRhiTexture::UnknownFormat ? QRhiTexture::RGBA8
                                                                                         : QRhiTexture::Format(rtd->u.nativeTexture.rhiFormat);
        const auto flags = QRhiTexture::RenderTarget | QRhiTexture::Flags(
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
                               rtd->u.nativeTexture.rhiFormatFlags
#else
                               rtd->u.nativeTexture.rhiFlags
#endif
                                                                          );
        std::unique_ptr<QRhiTexture> texture(rhi->newTexture(format, rtd->pixelSize, rtd->sampleCount, flags));
        texture->setName(QByteArrayLiteral("WaylibTexture"));
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layout }))
#else
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layoutOrState }))
#endif
            return false;
        QRhiColorAttachment att(texture.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst))
            return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        dst.res.texture = texture.release();
#else
        dst.texture = texture.release();
#endif
        return true;
    }
    case QQuickRenderTargetPrivate::Type::NativeRenderbuffer: {
        std::unique_ptr<QRhiRenderBuffer> renderbuffer(rhi->newRenderBuffer(QRhiRenderBuffer::Color, rtd->pixelSize, rtd->sampleCount));
        if (!renderbuffer->createFrom({ rtd->u.nativeRenderbufferObject })) {
            qCWarning(lcWlRenderHelper, "Failed to build wrapper renderbuffer for QQuickRenderTarget");
            return false;
        }
        QRhiColorAttachment att(renderbuffer.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst))
            return false;
        renderbuffer->setName(QByteArrayLiteral("WaylibRenderBuffer"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        dst.res.renderBuffer = renderbuffer.release();
#else
        dst.renderBuffer = renderbuffer.release();
#endif
        return true;
    }

    default:
        break;
    }

    return false;
}
// Copy end

class Q_DECL_HIDDEN WRenderHelperPrivate : public WObjectPrivate
{
public:
    WRenderHelperPrivate(WRenderHelper *qq, wlr_renderer *renderer)
        : WObjectPrivate(qq)
        , renderer(renderer)
    {}
    ~WRenderHelperPrivate() {
        resetRenderBuffer();
    }

    void resetRenderBuffer();
    void onBufferDestroy(wlr_buffer *buffer);
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data);

    W_DECLARE_PUBLIC(WRenderHelper)
    wlr_renderer *renderer;
    QList<BufferData*> buffers;
    BufferData *lastBuffer = nullptr;

    QSize size;
};

void WRenderHelperPrivate::resetRenderBuffer()
{
    qDeleteAll(buffers);
    lastBuffer = nullptr;
    buffers.clear();
}

void WRenderHelperPrivate::onBufferDestroy(wlr_buffer *buffer)
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

bool WRenderHelperPrivate::ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data)
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    data->renderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->windowRenderTarget.rt.renderTarget);
#else
    data->renderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->windowRenderTarget.renderTarget);
#endif
    data->renderTarget.setDevicePixelRatio(tmp.devicePixelRatio());
    data->renderTarget.setMirrorVertically(tmp.mirrorVertically());

    return true;
}

WRenderHelper::WRenderHelper(wlr_renderer *renderer, QObject *parent)
    : QObject(parent)
    , WObject(*new WRenderHelperPrivate(this, renderer))
{

}

QSize WRenderHelper::size() const
{
    W_DC(WRenderHelper);
    return d->size;
}

void WRenderHelper::setSize(const QSize &size)
{
    W_D(WRenderHelper);
    if (d->size == size)
        return;
    d->size = size;
    d->resetRenderBuffer();

    Q_EMIT sizeChanged();
}

QSGRendererInterface::GraphicsApi WRenderHelper::getGraphicsApi(QQuickRenderControl *rc)
{
    auto d = QQuickRenderControlPrivate::get(rc);
    return d->sg->rendererInterface(d->rc)->graphicsApi();
}

QSGRendererInterface::GraphicsApi WRenderHelper::getGraphicsApi()
{
    auto getApi = [] () {
        // Only for get GraphicsApi
        QQuickRenderControl rc;
        return getGraphicsApi(&rc);
    };

    static auto api = getApi();
    return api;
}

class Q_DECL_HIDDEN GLTextureBuffer
{
public:
    static wlr_buffer *create(wlr_egl *egl, QSGTexture *texture);

private:
    explicit GLTextureBuffer(wlr_egl *egl, QSGTexture *texture);
    static GLTextureBuffer *fromBuffer(wlr_buffer *buffer);
    static void destroy(wlr_buffer *buffer);
    static bool getDmabuf(wlr_buffer *buffer, wlr_dmabuf_attributes *attribs);

    wlr_buffer m_buffer;
    wlr_egl *m_egl;
    QSGTexture *m_texture;
};

GLTextureBuffer::GLTextureBuffer(wlr_egl *egl, QSGTexture *texture)
    : m_egl(egl)
    , m_texture(texture)
{
    static const wlr_buffer_impl impl {
        .destroy = destroy,
        .get_dmabuf = getDmabuf,
        .get_shm = nullptr,
        .begin_data_ptr_access = nullptr,
        .end_data_ptr_access = nullptr,
    };
    const QSize size = texture->textureSize();
    wlr_buffer_init(&m_buffer, &impl, size.width(), size.height());
}

wlr_buffer *GLTextureBuffer::create(wlr_egl *egl, QSGTexture *texture)
{
    return &(new GLTextureBuffer(egl, texture))->m_buffer;
}

GLTextureBuffer *GLTextureBuffer::fromBuffer(wlr_buffer *buffer)
{
    return reinterpret_cast<GLTextureBuffer *>(buffer);
}

void GLTextureBuffer::destroy(wlr_buffer *buffer)
{
    wlr_buffer_finish(buffer);
    delete fromBuffer(buffer);
}

bool GLTextureBuffer::getDmabuf(wlr_buffer *buffer, wlr_dmabuf_attributes *attribs)
{
    auto *self = fromBuffer(buffer);
    auto *rhiTexture = self->m_texture->rhiTexture();
    if (!rhiTexture)
        return false;

    auto display = wlr_egl_get_display(self->m_egl);
    auto context = wlr_egl_get_context(self->m_egl);

    EGLImage image = eglCreateImage(display, context,
                                    EGL_GL_TEXTURE_2D,
                                    reinterpret_cast<EGLClientBuffer>(rhiTexture->nativeTexture().object),
                                    nullptr);

    if (image == EGL_NO_IMAGE)
        return false;

    static auto eglExportDMABUFImageQueryMESA =
        reinterpret_cast<PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC>(eglGetProcAddress("eglExportDMABUFImageQueryMESA"));
    static auto eglExportDMABUFImageMESA =
        reinterpret_cast<PFNEGLEXPORTDMABUFIMAGEMESAPROC>(eglGetProcAddress("eglExportDMABUFImageMESA"));

    if (!eglExportDMABUFImageQueryMESA || !eglExportDMABUFImageMESA)
        return false;

    bool ok = eglExportDMABUFImageQueryMESA(display,
                                            image,
                                            reinterpret_cast<int*>(&attribs->format),
                                            &attribs->n_planes,
                                            &attribs->modifier);
    if (!ok)
        return false;

    ok = eglExportDMABUFImageMESA(display,
                                  image,
                                  attribs->fd,
                                  reinterpret_cast<int*>(attribs->stride),
                                  reinterpret_cast<int*>(attribs->offset));
    if (!ok)
        return false;

    attribs->width = buffer->width;
    attribs->height = buffer->height;

    return true;
}

#ifdef ENABLE_VULKAN_RENDER
class Q_DECL_HIDDEN VkTextureBuffer
{
public:
    static wlr_buffer *create(VkInstance instance, VkDevice device, QSGTexture *texture);

private:
    explicit VkTextureBuffer(VkInstance instance, VkDevice device, QSGTexture *texture);
    static VkTextureBuffer *fromBuffer(wlr_buffer *buffer);
    static void destroy(wlr_buffer *buffer);
    static bool getDmabuf(wlr_buffer *buffer, wlr_dmabuf_attributes *attribs);

    wlr_buffer m_buffer;
    [[maybe_unused]] VkInstance m_instance;
    [[maybe_unused]] VkDevice m_device;
    [[maybe_unused]] QSGTexture *m_texture;
};

VkTextureBuffer::VkTextureBuffer(VkInstance instance, VkDevice device, QSGTexture *texture)
    : m_instance(instance)
    , m_device(device)
    , m_texture(texture)
{
    static const wlr_buffer_impl impl {
        .destroy = destroy,
        .get_dmabuf = getDmabuf,
        .get_shm = nullptr,
        .begin_data_ptr_access = nullptr,
        .end_data_ptr_access = nullptr,
    };
    const QSize size = texture->textureSize();
    wlr_buffer_init(&m_buffer, &impl, size.width(), size.height());
}

wlr_buffer *VkTextureBuffer::create(VkInstance instance, VkDevice device, QSGTexture *texture)
{
    return &(new VkTextureBuffer(instance, device, texture))->m_buffer;
}

VkTextureBuffer *VkTextureBuffer::fromBuffer(wlr_buffer *buffer)
{
    return reinterpret_cast<VkTextureBuffer *>(buffer);
}

void VkTextureBuffer::destroy(wlr_buffer *buffer)
{
    wlr_buffer_finish(buffer);
    delete fromBuffer(buffer);
}

bool VkTextureBuffer::getDmabuf([[maybe_unused]] wlr_buffer *buffer,
                                [[maybe_unused]] wlr_dmabuf_attributes *attribs)
{
//    static auto vkGetInstanceProcAddr =
//        reinterpret_cast<PFN_vkGetInstanceProcAddr>(::dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr"));
//    static auto vkGetMemoryFdKHR =
//        reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetInstanceProcAddr(m_instance, "vkGetMemoryFdKHR"));
//    static auto vkGetImageMemoryRequirements =
//        reinterpret_cast<PFN_vkGetImageMemoryRequirements>(vkGetInstanceProcAddr(m_instance, "vkGetImageMemoryRequirements"));
//    static auto vkGetImageSparseMemoryRequirements =
//        reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(vkGetInstanceProcAddr(m_instance, "vkGetImageSparseMemoryRequirements"));
//    static auto vkGetImageSubresourceLayout =
//        reinterpret_cast<PFN_vkGetImageSubresourceLayout>(vkGetInstanceProcAddr(m_instance, "vkGetImageSubresourceLayout"));

    // TODO
    return false;
}
#endif

wlr_buffer *WRenderHelper::toBuffer(wlr_renderer *renderer, QSGTexture *texture,
                                    QSGRendererInterface::GraphicsApi api)
{
    const QSize size = texture->textureSize();

    switch (api) {
    case QSGRendererInterface::OpenGL: {
        Q_ASSERT(wlr_renderer_is_gles2(renderer));
        auto *egl = wlr_gles2_renderer_get_egl(renderer);

        return GLTextureBuffer::create(egl, texture);
    }
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        Q_ASSERT(wlr_renderer_is_vk(renderer));
        auto instance = wlr_vk_renderer_get_instance(renderer);
        auto device = wlr_vk_renderer_get_device(renderer);

        return VkTextureBuffer::create(instance, device, texture);
    }
#endif
    case QSGRendererInterface::Software: {
        QImage image;
        if (auto t = qobject_cast<QSGPlainTexture*>(texture)) {
            image = t->image();
        } else if (auto t = qobject_cast<QSGLayer*>(texture)) {
            image = t->toImage();
        } else if (QByteArrayView(texture->metaObject()->className())
                   == QByteArrayView("QSGSoftwarePixmapTexture")) {
            auto t = static_cast<QSGSoftwarePixmapTexture*>(texture);
            image = t->pixmap().toImage();
        } else {
            qFatal("Can't get QImage from QSGTexture, class name: %s", texture->metaObject()->className());
        }

        if (image.isNull())
            return nullptr;

        return WImageBufferImpl::create(image);
    }
    default:
        qFatal("Can't get wlr_buffer from QSGTexture, unsupported graphics API.");
        break;
    }

    return nullptr;
}

QQuickRenderTarget WRenderHelper::acquireRenderTarget(QQuickRenderControl *rc, wlr_buffer *buffer)
{
    W_D(WRenderHelper);
    Q_ASSERT(buffer);

    if (d->size.isEmpty())
        return {};

    for (int i = 0; i < d->buffers.count(); ++i) {
        auto data = d->buffers[i];
        if (data->buffer == buffer) {
            d->lastBuffer = data;
            return data->renderTarget;
        }
    }

    std::unique_ptr<BufferData> bufferData(new BufferData);
    bufferData->buffer = buffer;
    auto *texture = wlr_texture_from_buffer(d->renderer, buffer);
    if (!texture)
        return {};

    QQuickRenderTarget rt;

    if (wlr_renderer_is_pixman(d->renderer)) {
        pixman_image_t *image = wlr_pixman_texture_get_image(texture);
        void *data = pixman_image_get_data(image);
        if (bufferData->paintDevice.constBits() != data)
            bufferData->paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!bufferData->paintDevice.isNull());
        rt = QQuickRenderTarget::fromPaintDevice(&bufferData->paintDevice);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_renderer_is_vk(d->renderer)) {
        wlr_vk_image_attribs attribs;
        wlr_vk_texture_get_image_attribs(texture, &attribs);
        rt = QQuickRenderTarget::fromVulkanImage(attribs.image, attribs.layout, attribs.format, d->size);
    }
#endif
    else if (wlr_renderer_is_gles2(d->renderer)) {
        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(texture, &attribs);

        rt = QQuickRenderTarget::fromOpenGLTexture(attribs.tex, d->size);
        rt.setMirrorVertically(true);
    }

    wlr_texture_destroy(texture);
    bufferData->renderTarget = rt;

    if (QSGRendererInterface::isApiRhiBased(getGraphicsApi(rc))) {
        if (!rt.isNull()) {
            // Force convert to Rhi render target
            if (!d->ensureRhiRenderTarget(rc, bufferData.get()))
                bufferData->renderTarget = {};
        }

        if (bufferData->renderTarget.isNull())
            return {};

        if (auto texture = bufferData->windowRenderTarget.res.texture) {
            s_rhiRenderBuffers->append({ bufferData->windowRenderTarget.rt.renderTarget,
                                         texture, bufferData->buffer });
        }
    }

    bufferData->destroyListener.connect(&buffer->events.destroy, [d, buffer](void *) {
        d->onBufferDestroy(buffer);
    });

    d->buffers.append(bufferData.release());
    d->lastBuffer = d->buffers.last();

    return d->buffers.last()->renderTarget;
}

std::pair<wlr_buffer *, QQuickRenderTarget> WRenderHelper::lastRenderTarget() const
{
    W_DC(WRenderHelper);
    if (!d->lastBuffer)
        return {nullptr, {}};

    return {d->lastBuffer->buffer, d->lastBuffer->renderTarget};
}

static wlr_renderer *createRendererWithType(const char *type, wlr_backend *backend)
{
    qputenv("WLR_RENDERER", type);
    auto *render = wlr_renderer_autocreate(backend);
    qunsetenv("WLR_RENDERER");

    return render;
}

wlr_renderer *WRenderHelper::createRenderer(wlr_backend *backend)
{
    auto api = getGraphicsApi();
    return createRenderer(backend, api);
}

wlr_renderer *WRenderHelper::createRenderer(wlr_backend *backend, QSGRendererInterface::GraphicsApi api)
{
    wlr_renderer *renderer = nullptr;
    switch (api) {
    case QSGRendererInterface::OpenGL:
        renderer = createRendererWithType("gles2", backend);
        Q_ASSERT(!renderer || wlr_renderer_is_gles2(renderer));
        break;
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        renderer = createRendererWithType("vulkan", backend);
        Q_ASSERT(!renderer || wlr_renderer_is_vk(renderer));
        break;
    }
#endif
    case QSGRendererInterface::Software:
        renderer = createRendererWithType("pixman", backend);
        Q_ASSERT(!renderer || wlr_renderer_is_pixman(renderer));
        break;
    default:
        qFatal("Not supported graphics api: %s", qPrintable(QQuickWindow::sceneGraphBackend()));
        break;
    }

    return renderer;
}

constexpr const char *GraphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
        using enum QSGRendererInterface::GraphicsApi;
    case Software:
        return "Software";
    case OpenGL:
        return "OpenGL";
    case Vulkan:
        return "Vulkan";
    default:
        return "Unknown/Unsupported";
    }
}

void WRenderHelper::setupRendererBackend(wlr_backend *testBackend)
{
    const auto wlrRenderer = qgetenv("WLR_RENDERER");

    if (wlrRenderer == "auto" || wlrRenderer.isEmpty()) {
        if (qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
            || (qEnvironmentVariableIsSet("QT_QUICK_BACKEND")
                && qgetenv("QT_QUICK_BACKEND") != "rhi")) {
            // when environment variable Q*_BACKEND was set, should defer to
            // the env variable for the graphics API.
            return;
        }

        QList<QSGRendererInterface::GraphicsApi> apiList = {
            QSGRendererInterface::OpenGL,
            QSGRendererInterface::Software
            // TODO: Add vulkan to list.
        };
        std::unique_ptr<wl_display, decltype(&wl_display_destroy)> display { nullptr, wl_display_destroy };
        if (!testBackend) {
            display.reset(wl_display_create());
            testBackend = wlr_backend_autocreate(wl_display_get_event_loop(display.get()), nullptr);

            if (!testBackend)
                qFatal("Failed to create wlr_backend");

            wlr_backend_start(testBackend);
        }
        QQuickWindow::setGraphicsApi(WRenderHelper::probe(testBackend, apiList));
    } else if (wlrRenderer == "gles2") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    } else if (wlrRenderer == "vulkan") {
#ifdef ENABLE_VULKAN_RENDER
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
#else
        qFatal("Vulkan support is not enabled");
#endif
    } else if (wlrRenderer == "pixman") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    } else {
        qFatal() << "Unknown/Unsupported wlr renderer: " << wlrRenderer;
    }
}

QSGRendererInterface::GraphicsApi WRenderHelper::probe(wlr_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList)
{
    auto acceptApi = QSGRendererInterface::Unknown;

    struct RendererDestroyer {
        void operator()(wlr_renderer *renderer) const { wlr_renderer_destroy(renderer); }
    };
    struct AllocatorDestroyer {
        void operator()(wlr_allocator *allocator) const { wlr_allocator_destroy(allocator); }
    };
    struct SwapchainDestroyer {
        void operator()(wlr_swapchain *swapchain) const { wlr_swapchain_destroy(swapchain); }
    };
    struct BufferUnlocker {
        void operator()(wlr_buffer *buffer) const { wlr_buffer_unlock(buffer); }
    };
    struct TextureDestroyer {
        void operator()(wlr_texture *texture) const { wlr_texture_destroy(texture); }
    };

    for (auto api : std::as_const(apiList)) {
        std::unique_ptr<wlr_renderer, RendererDestroyer> renderer(createRenderer(testBackend, api));
        if (!renderer) {
            qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api failed to create wlr_renderer";
            continue;
        }

        const wlr_drm_format_set *formats =
            wlr_renderer_get_texture_formats(renderer.get(), WLR_BUFFER_CAP_DMABUF);

        if (formats && formats->len == 0) {
            qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api don't support any format";
            continue;
        }

        // TODO: how to test when formats gets NULL
        if (formats && formats->len) {
            std::unique_ptr<wlr_allocator, AllocatorDestroyer> alloc(
                wlr_allocator_autocreate(testBackend, renderer.get()));
            if (!alloc)
                continue;

            bool hasSupportedFormat = false;
            for (size_t formatId = 0; formatId < formats->len; formatId++) {
                auto *format = &formats->formats[formatId];

                std::unique_ptr<wlr_swapchain, SwapchainDestroyer> swapchain(
                    wlr_swapchain_create(alloc.get(), 1000, 800, format));
                if (!swapchain)
                    continue;
                auto *wbuffer = wlr_swapchain_acquire(swapchain.get());
                if (!wbuffer) {
                    continue;
                } else {
                    std::unique_ptr<wlr_buffer, BufferUnlocker> buffer(wbuffer);
                    std::unique_ptr<wlr_texture, TextureDestroyer> texture {
                        wlr_texture_from_buffer(renderer.get(), buffer.get())
                    };
                    if (!texture)
                        continue;
                    hasSupportedFormat = true;
                    break;
                }
            }

            if (!hasSupportedFormat) {
                qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api failed to convert any buffer to texture";
                continue;
            }
        }

        acceptApi = api;
        break;
    }

    return acceptApi;
}

static void updateGLTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture) {
    wlr_gles2_texture_attribs attribs;
    wlr_gles2_texture_get_attribs(handle, &attribs);
    QSize size(handle->width, handle->height);

#define GL_TEXTURE_EXTERNAL_OES           0x8D65
    QQuickWindowPrivate::TextureFromNativeTextureFlags flags = attribs.target == GL_TEXTURE_EXTERNAL_OES
                                                                   ? QQuickWindowPrivate::NativeTextureIsExternalOES
                                                                   : QQuickWindowPrivate::TextureFromNativeTextureFlags {};
    texture->setTextureFromNativeTexture(rhi, attribs.tex, 0, 0, size, {}, flags);

    texture->setHasAlphaChannel(attribs.has_alpha);
    texture->setTextureSize(size);
}

static inline quint64 vkimage_cast(void *image) {
    return reinterpret_cast<quintptr>(image);
}

[[maybe_unused]] static inline quint64 vkimage_cast(quint64 image) {
    return image;
}

#ifdef ENABLE_VULKAN_RENDER
static void updateVKTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture) {
    wlr_vk_image_attribs attribs;
    wlr_vk_texture_get_image_attribs(handle, &attribs);
    QSize size(handle->width, handle->height);

    texture->setTextureFromNativeTexture(rhi,
                                         vkimage_cast(attribs.image),
                                         attribs.layout, attribs.format, size,
                                         {}, {});
    texture->setHasAlphaChannel(wlr_vk_texture_has_alpha(handle));
    texture->setTextureSize(size);
}
#endif

static void updateImage(QRhi *, wlr_texture *handle, QSGPlainTexture *texture) {
    auto image = wlr_pixman_texture_get_image(handle);
    texture->setImage(WTools::fromPixmanImage(image));
}

typedef void(*UpdateTextureFunction)(QRhi *, wlr_texture *, QSGPlainTexture *);

static UpdateTextureFunction getUpdateTextFunction(wlr_texture *handle)
{
    const auto api = WRenderHelper::getGraphicsApi();
    if (api == QSGRendererInterface::OpenGL) {
        Q_ASSERT(wlr_texture_is_gles2(handle));
        return updateGLTexture;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (api == QSGRendererInterface::Vulkan) {
        Q_ASSERT(wlr_texture_is_vk(handle));
        return updateVKTexture;
    }
#endif
    else if (api == QSGRendererInterface::Software) {
        Q_ASSERT(wlr_texture_is_pixman(handle));
        return updateImage;
    }

    return nullptr;
}

bool WRenderHelper::makeTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture)
{
    auto updateTexture = getUpdateTextFunction(handle);
    if (Q_UNLIKELY(!updateTexture))
        return false;
    updateTexture(rhi, handle, texture);
    return true;
}

WRenderHelper::TextureEntry
WRenderHelper::newTexture(wlr_allocator *allocator, wlr_renderer *renderer,
                          uint32_t drmFormat, uint64_t drmModifier,
                          QRhi *rhi, const QSize &size,
                          int rhiFormat, int rhiFlags)
{
    uint64_t modifiers[] = {drmModifier};
    wlr_drm_format format {
        .format = drmFormat,
        .len = 1,
        .capacity = 1,
        .modifiers = modifiers
    };

    wlr_buffer *buffer = wlr_allocator_create_buffer(
        allocator, size.width(), size.height(), &format);
    if (!buffer) {
        qCCritical(lcWlRenderHelper) << "Failed to create wlr_buffer from allocator";
        return {};
    }

    std::unique_ptr<wlr_texture, decltype(&wlr_texture_destroy)> texture(
        wlr_texture_from_buffer(renderer, buffer), wlr_texture_destroy);
    if (!texture) {
        qCCritical(lcWlRenderHelper) << "Failed to create wlr_texture from buffer";
        wlr_buffer_drop(buffer);
        return {};
    }

    const auto qformat = static_cast<QRhiTexture::Format>(rhiFormat);
    const auto qflags = QRhiTexture::Flags(rhiFlags);
    std::unique_ptr<QRhiTexture> rhiTexture(rhi->newTexture(qformat, size, 1, qflags));

    if (wlr_texture_is_gles2(texture.get())) {
        if (rhi->backend() != QRhi::OpenGLES2) {
            qFatal("The current QRhi backend doesn't support creating texture from GLES2 texture");
        }

        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(texture.get(), &attribs);

        if (!rhiTexture->createFrom({attribs.tex, 0})) {
            qCCritical(lcWlRenderHelper, "Failed to create QRhiTexture from GLES2 texture");
            wlr_buffer_drop(buffer);
            return {};
        }
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_texture_is_vk(texture.get())) {
        if (rhi->backend() != QRhi::Vulkan) {
            qFatal("The current QRhi backend doesn't support creating texture from Vulkan image");
        }

        wlr_vk_image_attribs attribs;
        wlr_vk_texture_get_image_attribs(texture.get(), &attribs);

        if (!rhiTexture->createFrom({vkimage_cast(attribs.image), attribs.layout})) {
            qCCritical(lcWlRenderHelper, "Failed to create QRhiTexture from Vulkan image");
            wlr_buffer_drop(buffer);
            return {};
        }
    }
#endif
    else if (wlr_texture_is_pixman(texture.get())) {
        qFatal("Creating QRhiTexture from Pixman image is not supported");
    } else {
        qFatal("Unknown texture type");
    }

    rhiTexture->setName("WaylibTexture");

    return {buffer, texture.release(), rhiTexture.release()};
}

WRenderHelper::TextureEntry
WRenderHelper::newTextureLike(wlr_allocator *allocator,
                              wlr_renderer *renderer,
                              QRhiTexture *texture, QRhi *rhi,
                              int rhiFlags)
{
    auto buffer = lookupBuffer(texture);
    if (!buffer)
        return {};

    wlr_dmabuf_attributes attribs;
    if (!wlr_buffer_get_dmabuf(buffer, &attribs))
        return {};

    return newTexture(allocator, renderer, attribs.format, attribs.modifier,
                      rhi, texture->pixelSize(), texture->format(), rhiFlags);
}

wlr_buffer *WRenderHelper::lookupBuffer(const QRhiRenderTarget *rt)
{
    for (const auto &entry : std::as_const(*s_rhiRenderBuffers)) {
        if (entry.renderTarget == rt)
            return entry.buffer;
    }

    return nullptr;
}

wlr_buffer *WRenderHelper::lookupBuffer(const QRhiTexture *texture)
{
    for (const auto &entry : std::as_const(*s_rhiRenderBuffers)) {
        if (entry.texture == texture)
            return entry.buffer;
    }

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wrenderhelper.cpp"
