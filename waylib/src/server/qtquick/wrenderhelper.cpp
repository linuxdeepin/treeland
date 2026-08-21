// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderhelper.h"
#include "wtools.h"
#include "wscoplistener.h"
#include "wpointer.h"
#include "wayliblogging.h"
#include "private/wqmlhelper_p.h"
#include "private/wglobal_p.h"
#include <memory>

#include <wlr_all.h>
#include <wcontainerof.h>

#include <QSGTexture>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qsgplaintexture_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qsgsoftwarepixmaptexture_p.h>
#include <private/qsgrhisupport_p.h>
#ifdef ENABLE_VULKAN_RENDER
#include <rhi/qrhi_platform.h>
#endif

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
    BufferData() = default;

    ~BufferData() {
        resetWindowRenderTarget();
    }

    WPointer<wlr_buffer> buffer;
    std::unique_ptr<WListenerOwner> bufferListenerOwner;
    // for software renderer
    WImageRenderTarget paintDevice;
    QQuickRenderTarget renderTarget;
    QQuickWindowRenderTarget windowRenderTarget;
    bool colorPreserved = false;

    inline void resetWindowRenderTarget() {
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
    }
};

class WRenderHelper::RenderTarget::Private {
public:
    std::weak_ptr<BufferData> data;
};

WRenderHelper::RenderTarget::RenderTarget() : d(new Private) {}
WRenderHelper::RenderTarget::RenderTarget(const RenderTarget &other)
    : d(other.d ? new Private(*other.d) : nullptr) {}
WRenderHelper::RenderTarget &WRenderHelper::RenderTarget::operator=(const RenderTarget &other)
{
    if (this != &other) {
        delete d;
        d = other.d ? new Private(*other.d) : nullptr;
    }
    return *this;
}
WRenderHelper::RenderTarget::~RenderTarget() { delete d; }

bool WRenderHelper::RenderTarget::isNull() const
{
    return !d || d->data.expired();
}

QQuickRenderTarget WRenderHelper::RenderTarget::rt() const
{
    if (!d)
        return {};
    auto data = d->data.lock();
    return data ? data->renderTarget : QQuickRenderTarget();
}

wlr_buffer *WRenderHelper::RenderTarget::buffer() const
{
    if (!d)
        return nullptr;
    auto data = d->data.lock();
    return data ? data->buffer.get() : nullptr;
}

bool WRenderHelper::RenderTarget::colorPreserved() const
{
    if (!d)
        return false;
    auto data = d->data.lock();
    return data ? data->colorPreserved : false;
}

static constexpr WGlobal::ColorContentsMode resolveColorContentsMode(
    WGlobal::ColorContentsMode requested, bool softwareRenderer) noexcept
{
    if (requested != WGlobal::ColorContentsMode::DontCare)
        return requested;
    // Software clear is expensive; default to preserve.
    return softwareRenderer ? WGlobal::ColorContentsMode::Preserve
                            : WGlobal::ColorContentsMode::Clear;
}

static QRhiTextureRenderTarget::Flags rhiRenderTargetFlags(WGlobal::ColorContentsMode mode)
{
    Q_ASSERT(mode != WGlobal::ColorContentsMode::DontCare);
    return mode == WGlobal::ColorContentsMode::Preserve
        ? QRhiTextureRenderTarget::PreserveColorContents
        : QRhiTextureRenderTarget::Flags{};
}

static bool recreateRhiRenderTarget(BufferData *data, QRhiTextureRenderTarget::Flags flags)
{
    auto renderTarget = static_cast<QRhiTextureRenderTarget *>(
        data->windowRenderTarget.rt.renderTarget);
    auto &rpDesc = data->windowRenderTarget.res.rpDesc;
    Q_ASSERT(renderTarget);
    renderTarget->destroy();
    renderTarget->setFlags(flags);

    auto newRpDesc = renderTarget->newCompatibleRenderPassDescriptor();
    if (!newRpDesc)
        return false;
    delete rpDesc;
    rpDesc = newRpDesc;
    renderTarget->setRenderPassDescriptor(rpDesc);
    return renderTarget->create();
}

// Copy from qquickrendertarget.cpp
static bool createRhiRenderTarget(const QRhiColorAttachment &colorAttachment,
                                  const QSize &pixelSize,
                                  int sampleCount,
                                  QRhi *rhi,
                                  QQuickWindowRenderTarget &dst,
                                  QRhiTextureRenderTarget::Flags flags)
{
    std::unique_ptr<QRhiRenderBuffer> depthStencil(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, sampleCount));
    if (!depthStencil->create()) {
        qCWarning(lcWlRenderHelper, "Failed to build depth-stencil buffer for QQuickRenderTarget");
        return false;
    }

    QRhiTextureRenderTargetDescription rtDesc(colorAttachment);
    rtDesc.setDepthStencilBuffer(depthStencil.get());
    std::unique_ptr<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc, flags));
    std::unique_ptr<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());

    if (!rt->create()) {
        qCWarning(lcWlRenderHelper, "Failed to build texture render target for QQuickRenderTarget");
        return false;
    }

    rt->setName(QByteArrayLiteral("WaylibTextureRenderTarget"));
    dst.rt.renderTarget = rt.release();
    dst.res.rpDesc = rp.release();
    dst.implicitBuffers.depthStencil = depthStencil.release();
    dst.rt.owns = true; // ownership of the native resource itself is not transferred but the QRhi objects are on us now
    return true;
}

bool createRhiRenderTarget(QRhi *rhi, const QQuickRenderTarget &source, QQuickWindowRenderTarget &dst,
                           QRhiTextureRenderTarget::Flags flags)
{
    auto rtd = QQuickRenderTargetPrivate::get(&source);

    switch (rtd->type) {
    case QQuickRenderTargetPrivate::Type::NativeTexture: {
        const auto format = rtd->u.nativeTexture.rhiFormat == QRhiTexture::UnknownFormat ? QRhiTexture::RGBA8
                                                                                         : QRhiTexture::Format(rtd->u.nativeTexture.rhiFormat);
        const auto textureFlags = QRhiTexture::RenderTarget | QRhiTexture::Flags(
                               rtd->u.nativeTexture.rhiFormatFlags
                                                                          );
        std::unique_ptr<QRhiTexture> texture(rhi->newTexture(format, rtd->pixelSize, rtd->sampleCount, textureFlags));
        texture->setName(QByteArrayLiteral("WaylibTexture"));
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layoutOrState }))
            return false;
        QRhiColorAttachment att(texture.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst, flags))
            return false;
        dst.res.texture = texture.release();
        return true;
    }
    case QQuickRenderTargetPrivate::Type::NativeRenderbuffer: {
        std::unique_ptr<QRhiRenderBuffer> renderbuffer(rhi->newRenderBuffer(QRhiRenderBuffer::Color, rtd->pixelSize, rtd->sampleCount));
        if (!renderbuffer->createFrom({ rtd->u.nativeRenderbufferObject })) {
            qCWarning(lcWlRenderHelper, "Failed to build wrapper renderbuffer for QQuickRenderTarget");
            return false;
        }
        QRhiColorAttachment att(renderbuffer.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst, flags))
            return false;
        renderbuffer->setName(QByteArrayLiteral("WaylibRenderBuffer"));
        dst.res.renderBuffer = renderbuffer.release();
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
    void onBufferDestroy(BufferData *data);
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data,
                                      QRhiTextureRenderTarget::Flags flags);

    W_DECLARE_PUBLIC(WRenderHelper)
    wlr_renderer *renderer;
    QList<std::shared_ptr<BufferData>> buffers;
    std::weak_ptr<BufferData> lastBuffer;

    QSize size;
};

void WRenderHelperPrivate::resetRenderBuffer()
{
    buffers.clear();
    lastBuffer.reset();
}

void WRenderHelperPrivate::onBufferDestroy(BufferData *data)
{
    // wlr_buffer_finish asserts the destroy/release listener lists are empty
    // right after emitting destroy; detach while handling it.
    if (data->bufferListenerOwner)
        data->bufferListenerOwner->removeListeners(data->bufferListenerOwner.get());
    for (int i = 0; i < buffers.count(); ++i) {
        auto entry = buffers[i];
        if (entry.get() == data) {
            auto locked = lastBuffer.lock();
            if (locked && locked == entry)
                lastBuffer.reset();
            buffers.removeAt(i);
            break;
        }
    }
}

bool WRenderHelperPrivate::ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data,
                                                 QRhiTextureRenderTarget::Flags flags)
{
    data->resetWindowRenderTarget();
    auto rhi = rc->rhi();
    auto tmp = data->renderTarget;
    bool ok = createRhiRenderTarget(rhi, tmp, data->windowRenderTarget, flags);
    if (!ok)
        return false;
    data->renderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->windowRenderTarget.rt.renderTarget);
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
    GLTextureBuffer(wlr_egl *egl, QSGTexture *texture, int width, int height);

    wlr_buffer *handle() { return &buffer; }

private:
    static const struct wlr_buffer_impl impl;
    static bool get_dmabuf(struct wlr_buffer *buffer, struct wlr_dmabuf_attributes *attribs);
    static void destroy(struct wlr_buffer *buffer);

    static inline GLTextureBuffer *fromBuffer(wlr_buffer *buffer) {
        if (buffer->impl != &GLTextureBuffer::impl)
            return nullptr;
        return W_CONTAINER_OF(buffer, GLTextureBuffer, buffer);
    }

    wlr_buffer buffer;
    wlr_egl *m_egl;
    QSGTexture *m_texture;
};

const struct wlr_buffer_impl GLTextureBuffer::impl = {
    .destroy = GLTextureBuffer::destroy,
    .get_dmabuf = GLTextureBuffer::get_dmabuf,
    .get_shm = NULL,
    .begin_data_ptr_access = NULL,
    .end_data_ptr_access = NULL,
};

GLTextureBuffer::GLTextureBuffer(wlr_egl *egl, QSGTexture *texture, int width, int height)
    : m_egl(egl)
    , m_texture(texture)
{
    wlr_buffer_init(&buffer, &impl, width, height);
}

bool GLTextureBuffer::get_dmabuf(struct wlr_buffer *buffer, wlr_dmabuf_attributes *attribs)
{
    auto *self = GLTextureBuffer::fromBuffer(buffer);
    Q_ASSERT(self);
    auto rhiTexture = self->m_texture->rhiTexture();
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

    if (!eglExportDMABUFImageQueryMESA || !eglExportDMABUFImageMESA) {
        eglDestroyImage(display, image);
        return false;
    }

    bool ok = eglExportDMABUFImageQueryMESA(display,
                                            image,
                                            reinterpret_cast<int*>(&attribs->format),
                                            &attribs->n_planes,
                                            &attribs->modifier);
    if (!ok) {
        eglDestroyImage(display, image);
        return false;
    }

    ok = eglExportDMABUFImageMESA(display,
                                  image,
                                  attribs->fd,
                                  reinterpret_cast<int*>(attribs->stride),
                                  reinterpret_cast<int*>(attribs->offset));
    if (!ok) {
        eglDestroyImage(display, image);
        return false;
    }

    attribs->width = self->buffer.width;
    attribs->height = self->buffer.height;

    eglDestroyImage(display, image);
    return true;
}

void GLTextureBuffer::destroy(struct wlr_buffer *buffer)
{
    auto *self = GLTextureBuffer::fromBuffer(buffer);
    Q_ASSERT(self);
    wlr_buffer_finish(buffer);
    delete self;
}

#ifdef ENABLE_VULKAN_RENDER
class Q_DECL_HIDDEN VkTextureBuffer
{
public:
    VkTextureBuffer(VkInstance instance, VkDevice device, QSGTexture *texture, int width, int height);

    wlr_buffer *handle() { return &buffer; }

private:
    static const struct wlr_buffer_impl impl;
    static bool get_dmabuf(struct wlr_buffer *buffer, struct wlr_dmabuf_attributes *attribs);
    static void destroy(struct wlr_buffer *buffer);

    static inline VkTextureBuffer *fromBuffer(wlr_buffer *buffer) {
        if (buffer->impl != &VkTextureBuffer::impl)
            return nullptr;
        return W_CONTAINER_OF(buffer, VkTextureBuffer, buffer);
    }

    wlr_buffer buffer;
    [[maybe_unused]] VkInstance m_instance;
    [[maybe_unused]] VkDevice m_device;
    [[maybe_unused]] QSGTexture *m_texture;
};

const struct wlr_buffer_impl VkTextureBuffer::impl = {
    .destroy = VkTextureBuffer::destroy,
    .get_dmabuf = VkTextureBuffer::get_dmabuf,
    .get_shm = NULL,
    .begin_data_ptr_access = NULL,
    .end_data_ptr_access = NULL,
};

VkTextureBuffer::VkTextureBuffer(VkInstance instance, VkDevice device, QSGTexture *texture, int width, int height)
    : m_instance(instance)
    , m_device(device)
    , m_texture(texture)
{
    wlr_buffer_init(&buffer, &impl, width, height);
}

bool VkTextureBuffer::get_dmabuf([[maybe_unused]] struct wlr_buffer *buffer, [[maybe_unused]] wlr_dmabuf_attributes *attribs)
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

void VkTextureBuffer::destroy(struct wlr_buffer *buffer)
{
    auto *self = VkTextureBuffer::fromBuffer(buffer);
    Q_ASSERT(self);
    wlr_buffer_finish(buffer);
    delete self;
}
#endif

class Q_DECL_HIDDEN QImageBuffer
{
public:
    QImageBuffer(const QImage &image);
    ~QImageBuffer();

    wlr_buffer *handle() { return &buffer; }

private:
    static const struct wlr_buffer_impl impl;
    static bool get_shm(struct wlr_buffer *buffer, struct wlr_shm_attributes *attribs);
    static bool begin_data_ptr_access(struct wlr_buffer *buffer, uint32_t flags,
                                      void **data, uint32_t *format, size_t *stride);
    static void end_data_ptr_access(struct wlr_buffer *buffer);
    static void destroy(struct wlr_buffer *buffer);

    static QImageBuffer *fromBuffer(wlr_buffer *buffer);

    // QImage is kept behind a raw pointer so this class stays standard-layout
    // (std::unique_ptr is not standard-layout in libstdc++), allowing
    // container_of recovery of the owner from the wlr_buffer.
    wlr_buffer buffer;
    QImage *m_image;
};

const struct wlr_buffer_impl QImageBuffer::impl = {
    .destroy = QImageBuffer::destroy,
    .get_dmabuf = NULL,
    .get_shm = QImageBuffer::get_shm,
    .begin_data_ptr_access = QImageBuffer::begin_data_ptr_access,
    .end_data_ptr_access = QImageBuffer::end_data_ptr_access,
};

QImageBuffer::QImageBuffer(const QImage &image)
    : buffer{}
    , m_image(new QImage(image))
{
    wlr_buffer_init(&buffer, &impl, m_image->width(), m_image->height());
}

QImageBuffer::~QImageBuffer()
{
    delete m_image;
}

QImageBuffer *QImageBuffer::fromBuffer(wlr_buffer *buffer)
{
    if (buffer->impl != &QImageBuffer::impl)
        return nullptr;
    return W_CONTAINER_OF(buffer, QImageBuffer, buffer);
}

bool QImageBuffer::get_shm(struct wlr_buffer *buffer, wlr_shm_attributes *attribs)
{
    auto *self = QImageBuffer::fromBuffer(buffer);
    Q_ASSERT(self);
    attribs->fd = 0;
    attribs->format = WTools::toDrmFormat(self->m_image->format());
    attribs->width = self->m_image->width();
    attribs->height = self->m_image->height();
    attribs->stride = self->m_image->bytesPerLine();
    return true;
}

bool QImageBuffer::begin_data_ptr_access(struct wlr_buffer *buffer, [[maybe_unused]] uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
    auto *self = QImageBuffer::fromBuffer(buffer);
    Q_ASSERT(self);
    *data = self->m_image->bits();
    *format = WTools::toDrmFormat(self->m_image->format());
    *stride = self->m_image->bytesPerLine();

    return true;
}

void QImageBuffer::end_data_ptr_access(struct wlr_buffer * /*buffer*/)
{
}

void QImageBuffer::destroy(struct wlr_buffer *buffer)
{
    auto *self = QImageBuffer::fromBuffer(buffer);
    Q_ASSERT(self);
    wlr_buffer_finish(buffer);
    delete self;
}

wlr_buffer *WRenderHelper::toBuffer(wlr_renderer *renderer, QSGTexture *texture, QSGRendererInterface::GraphicsApi api)
{
    const QSize size = texture->textureSize();

    switch (api) {
    case QSGRendererInterface::OpenGL: {
        Q_ASSERT(wlr_renderer_is_gles2(renderer));
        auto egl = wlr_gles2_renderer_get_egl(renderer);

        return (new GLTextureBuffer(egl, texture, size.width(), size.height()))->handle();
    }
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        Q_ASSERT(wlr_renderer_is_vk(renderer));
        auto instance = wlr_vk_renderer_get_instance(renderer);
        auto device = wlr_vk_renderer_get_device(renderer);

        return (new VkTextureBuffer(instance, device, texture, size.width(), size.height()))->handle();
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

        return (new QImageBuffer(image))->handle();
    }
    default:
        qFatal("Can't get wlr_buffer from QSGTexture, Not supported graphics API.");
        break;
    }

    return nullptr;
}

WRenderHelper::RenderTarget WRenderHelper::acquireRenderTarget(QQuickRenderControl *rc, wlr_buffer *buffer,
                                                               WGlobal::ColorContentsMode mode)
{
    W_D(WRenderHelper);
    Q_ASSERT(buffer);

    if (d->size.isEmpty())
        return {};

    const bool isSoftware = wlr_renderer_is_pixman(d->renderer);
    const auto resolvedMode = resolveColorContentsMode(mode, isSoftware);
    const bool needPreserve = resolvedMode == WGlobal::ColorContentsMode::Preserve;
    const auto flags = rhiRenderTargetFlags(resolvedMode);

    for (int i = 0; i < d->buffers.count(); ++i) {
        auto data = d->buffers[i];
        if (data->buffer == buffer) {
            if (needPreserve != data->colorPreserved) {
#ifdef ENABLE_VULKAN_RENDER
                if (wlr_renderer_is_vk(d->renderer)) {
                    qCWarning(lcWlRenderHelper)
                        << "Recreating Vulkan render target for buffer" << buffer
                        << "to change color preserved from" << data->colorPreserved
                        << "to" << needPreserve;
                    if (!recreateRhiRenderTarget(data.get(), flags))
                        return {};
                } else
#endif
                {
                    auto renderTarget = data->windowRenderTarget.rt.renderTarget;
                    if (renderTarget)
                        static_cast<QRhiTextureRenderTarget *>(renderTarget)->setFlags(flags);
                }
            }
            data->colorPreserved = needPreserve;
            d->lastBuffer = data;
            RenderTarget result;
            result.d->data = data;
            return result;
        }
    }

    std::unique_ptr<BufferData> bufferData(new BufferData);
    bufferData->buffer = buffer;
    bufferData->colorPreserved = needPreserve;

    QQuickRenderTarget rt;

    if (isSoftware) {
        WUniquePointer<wlr_texture> texture(
            wlr_texture_from_buffer(d->renderer, buffer));
        if (!texture)
            return {};
        pixman_image_t *image = wlr_pixman_texture_get_image(texture.get());
        void *data = pixman_image_get_data(image);
        if (bufferData->paintDevice.constBits() != data)
            bufferData->paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!bufferData->paintDevice.isNull());
        rt = QQuickRenderTarget::fromPaintDevice(&bufferData->paintDevice);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_renderer_is_vk(d->renderer)) {
        // Reuse wlroots' wlr_vk_render_buffer (same VkImage as tinywl/scene),
        // not a parallel dmabuf import owned by waylib.
        wlr_vk_image_attribs attribs = {};
        if (!waylib_vk_renderer_get_render_buffer_attribs(d->renderer, buffer, &attribs))
            return {};
        rt = QQuickRenderTarget::fromVulkanImage(attribs.image,
                                                 attribs.layout,
                                                 attribs.format,
                                                 d->size);
    }
#endif
    else if (wlr_renderer_is_gles2(d->renderer)) {
        WUniquePointer<wlr_texture> texture(
            wlr_texture_from_buffer(d->renderer, buffer));
        if (!texture)
            return {};
        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(texture.get(), &attribs);

        rt = QQuickRenderTarget::fromOpenGLTexture(attribs.tex, d->size);
        rt.setMirrorVertically(true);
    }

    bufferData->renderTarget = rt;

    if (QSGRendererInterface::isApiRhiBased(getGraphicsApi(rc))) {
        if (!rt.isNull()) {
            // Force convert to Rhi render target
            if (!d->ensureRhiRenderTarget(rc, bufferData.get(), flags))
                bufferData->renderTarget = {};
        }

        if (bufferData->renderTarget.isNull())
            return {};

        if (auto texture = bufferData->windowRenderTarget.res.texture) {
            s_rhiRenderBuffers->append({ bufferData->windowRenderTarget.rt.renderTarget,
                                         texture, bufferData->buffer.get() });
        }
    }

    if (!bufferData->bufferListenerOwner)
        bufferData->bufferListenerOwner = std::make_unique<WListenerOwner>();
    auto *owner = bufferData->bufferListenerOwner.get();
    owner->listeners()->add(&buffer->events.destroy, this, [d, bufferData = bufferData.get(), owner] (void *) {
        owner->removeListeners(owner);
        d->onBufferDestroy(bufferData);
    });

    d->buffers.append(std::shared_ptr<BufferData>(bufferData.release()));
    d->lastBuffer = d->buffers.last();

    RenderTarget result;
    result.d->data = d->buffers.last();
    return result;
}

WRenderHelper::RenderTarget WRenderHelper::lastRenderTarget() const
{
    W_DC(WRenderHelper);
    auto data = d->lastBuffer.lock();
    if (!data)
        return {};

    RenderTarget result;
    result.d->data = data;
    return result;
}

#ifdef ENABLE_VULKAN_RENDER
void WRenderHelper::prepareVulkanRenderTarget(QRhiCommandBuffer *cb, const RenderTarget &rt)
{
    W_D(WRenderHelper);
    if (!rt.d || !cb)
        return;
    auto data = rt.d->data.lock();
    if (!data || !data->buffer || !d->renderer || !wlr_renderer_is_vk(d->renderer))
        return;

    cb->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(cb->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        cb->endExternal();
        qCWarning(lcWlRenderHelper, "Vulkan render buffer acquire: missing native command buffer");
        return;
    }

    // FOREIGN_EXT -> graphics queue, same as wlroots pass.c / PR #1171.
    if (!waylib_vk_renderer_record_render_buffer_acquire(d->renderer,
                                                         data->buffer.get(),
                                                         handles->commandBuffer)) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed for" << data->buffer.get();
    }
    cb->endExternal();
}

void WRenderHelper::finishVulkanRenderTarget(QRhiCommandBuffer *cb, const RenderTarget &rt)
{
    W_D(WRenderHelper);
    if (!rt.d || !cb)
        return;
    auto data = rt.d->data.lock();
    if (!data || !data->buffer || !d->renderer || !wlr_renderer_is_vk(d->renderer))
        return;

    QRhiTexture *qtTexture = data->windowRenderTarget.res.texture;
    if (!qtTexture) {
        qCWarning(lcWlRenderHelper, "Vulkan render buffer release: missing Qt QRhiTexture");
        return;
    }

    const auto native = qtTexture->nativeTexture();
    const auto oldLayout = VkImageLayout(native.layout);
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        qCWarning(lcWlRenderHelper, "Vulkan render buffer release: Qt texture layout is UNDEFINED");
        return;
    }

    cb->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(cb->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        cb->endExternal();
        qCWarning(lcWlRenderHelper, "Vulkan render buffer release: missing native command buffer");
        return;
    }

    // graphics queue -> FOREIGN_EXT + GENERAL for KMS, using Qt's real old layout.
    if (!waylib_vk_renderer_record_render_buffer_release(d->renderer,
                                                         data->buffer.get(),
                                                         handles->commandBuffer,
                                                         oldLayout)) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed for" << data->buffer.get();
        cb->endExternal();
        return;
    }
    cb->endExternal();

    // Keep QRhi's external-image tracker in sync with the post-release layout.
    qtTexture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);
}
#endif // ENABLE_VULKAN_RENDER

static wlr_renderer *createRendererWithType(const char *type, wlr_backend *backend)
{
    qputenv("WLR_RENDERER", type);
    auto render = wlr_renderer_autocreate(backend);
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
        wl_display *display = nullptr;
        if (!testBackend) {
            display = wl_display_create();
            Q_ASSERT(display);
            testBackend = wlr_backend_autocreate(wl_display_get_event_loop(display), nullptr);

            if (!testBackend)
                qFatal("Failed to create wlr_backend");

            wlr_backend_start(testBackend);
        }
        QQuickWindow::setGraphicsApi(WRenderHelper::probe(testBackend, apiList));

        if (display) {
            wlr_backend_destroy(testBackend);
            wl_display_destroy(display);
        }
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

    for (auto api : std::as_const(apiList)) {
        WUniquePointer<wlr_renderer> renderer(createRenderer(testBackend, api));
        if (!renderer) {
            qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api failed to create wlr_renderer";
            continue;
        }

        const wlr_drm_format_set *formats = wlr_renderer_get_texture_formats(renderer.get(), WLR_BUFFER_CAP_DMABUF);

        if (formats && formats->len == 0) {
            qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api don't support any format";
            continue;
        }

        // TODO: how to test when formats gets NULL
        if (formats && formats->len) {
            WUniquePointer<wlr_allocator> alloc(wlr_allocator_autocreate(testBackend, renderer.get()));

            bool hasSupportedFormat = false;
            for (size_t formatId = 0; formatId < formats->len; formatId++) {
                auto *format = &formats->formats[formatId];

                WUniquePointer<wlr_swapchain> swapchain(wlr_swapchain_create(alloc.get(), 1000, 800, format));
                struct wlr_buffer *wbuffer = wlr_swapchain_acquire(swapchain.get());
                if (!wbuffer) {
                    continue;
                } else {
                    WBufferUnlockPtr buffer(wbuffer);
                    WUniquePointer<wlr_texture> texture(wlr_texture_from_buffer(renderer.get(), buffer.get()));
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

    wlr_buffer *buffer = wlr_allocator_create_buffer(allocator, size.width(), size.height(), &format);
    if (!buffer) {
        qCCritical(lcWlRenderHelper) << "Failed to create wlr_buffer from allocator";
        return {};
    }

    WUniquePointer<wlr_texture> texture(wlr_texture_from_buffer(renderer, buffer));
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
