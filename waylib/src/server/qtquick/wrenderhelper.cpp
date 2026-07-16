// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderhelper.h"
#include "wtools.h"
#include "wayliblogging.h"
#include "private/wqmlhelper_p.h"
#include "private/wglobal_p.h"
#include "utils/private/wvulkantrace_p.h"

#include <qwbackend.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwbuffer.h>
#include <qwtexture.h>
#include <qwbufferinterface.h>
#include <qwdisplay.h>
#include <qwegl.h>
#include <qwallocator.h>
#include <qwrendererinterface.h>
#ifdef ENABLE_VULKAN_RENDER
#include <qwvulkan.h>
#endif

#include <QSGTexture>
#include <QVulkanInstance>
#include <rhi/qrhi_platform.h>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#ifdef ENABLE_VULKAN_RENDER
#include <private/qrhivulkan_p.h>
#endif
#include <private/qsgplaintexture_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qsgsoftwarepixmaptexture_p.h>
#include <private/qsgrhisupport_p.h>

#ifdef ENABLE_VULKAN_RENDER
#include <QVulkanFunctions>
#include <vulkan/vulkan.h>
#endif

extern "C" {
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
}
#include <drm_fourcc.h>

#include <type_traits>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN RhiRenderEntry {
    const QRhiRenderTarget *renderTarget;
    const QRhiTexture *texture;
    QPointer<qw_buffer> buffer;
};

Q_GLOBAL_STATIC(QVector<RhiRenderEntry>, s_rhiRenderBuffers)

#ifdef ENABLE_VULKAN_RENDER
static QString hex32(uint32_t value)
{
    return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0'));
}

static QString hex64(uint64_t value)
{
    return QStringLiteral("0x%1").arg(value, 16, 16, QLatin1Char('0'));
}

static quint64 vkImageValue(VkImage image)
{
    if constexpr (std::is_pointer_v<VkImage>) {
        return quint64(reinterpret_cast<quintptr>(image));
    } else {
        return quint64(image);
    }
}

static QString vkImageName(VkImage image)
{
    return hex64(vkImageValue(image));
}

static const char *vkImageLayoutName(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return "UNDEFINED";
    case VK_IMAGE_LAYOUT_GENERAL:
        return "GENERAL";
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return "COLOR_ATTACHMENT_OPTIMAL";
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return "SHADER_READ_ONLY_OPTIMAL";
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return "TRANSFER_SRC_OPTIMAL";
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return "TRANSFER_DST_OPTIMAL";
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return "PREINITIALIZED";
    default:
        return "UNKNOWN";
    }
}

static QSize wlrBufferSize(qw_buffer *buffer)
{
    return QSize(buffer->handle()->width, buffer->handle()->height);
}

static QSize wlrTextureSize(qw_texture *texture)
{
    return QSize(texture->handle()->width, texture->handle()->height);
}

static bool getVulkanRenderBufferAttribs(qw_renderer *renderer,
                                         qw_buffer *buffer,
                                         const char *purpose,
                                         wlr_vk_image_attribs *attribs)
{
    if (!renderer || !buffer || !qw_vulkan::isRenderer(renderer))
        return false;

    if (qw_vulkan::renderBufferAttribs(renderer, buffer, attribs)) {
        qCDebug(lcWlRenderHelper) << "Got wlroots Vulkan render buffer attributes"
                                  << "purpose" << purpose
                                  << "qwBuffer" << buffer
                                  << "wlrBuffer" << buffer->handle()
                                  << "image" << vkImageName(attribs->image)
                                  << "layout" << vkImageLayoutName(attribs->layout)
                                  << "format" << hex32(attribs->format)
                                  << "size" << wlrBufferSize(buffer);
        return true;
    }

    qCWarning(lcWlRenderHelper) << "Failed to get wlroots Vulkan render buffer attributes"
                                << "purpose" << purpose
                                << "qwBuffer" << buffer
                                << "wlrBuffer" << buffer->handle()
                                << "size" << wlrBufferSize(buffer);
    return false;
}
#endif

struct Q_DECL_HIDDEN BufferData {
    BufferData() {

    }

    ~BufferData() {
        resetWindowRenderTarget();
    }

    qw_buffer *buffer = nullptr;
    // for software renderer
    WImageRenderTarget paintDevice;
    QQuickRenderTarget renderTarget;
    QQuickWindowRenderTarget windowRenderTarget;
    QQuickRenderTarget preserveRenderTarget;
    QQuickWindowRenderTarget preserveWindowRenderTarget;

    static inline void cleanupWindowRenderTarget(QQuickWindowRenderTarget &target) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        {
            auto it = s_rhiRenderBuffers->begin();
            while (it != s_rhiRenderBuffers->end()) {
                if (target.rt.renderTarget == it->renderTarget) {
                    it = s_rhiRenderBuffers->erase(it);
                    break;
                }
                ++it;
            }
        }

        if (target.rt.owns)
            delete target.rt.renderTarget;

        delete target.res.texture;
        delete target.res.renderBuffer;
        delete target.res.rpDesc;

        target.rt = {};
        target.res = {};
        { // target.implicitBuffers.reset(rhi);
            delete target.implicitBuffers.depthStencil;
            delete target.implicitBuffers.depthStencilTexture;
            delete target.implicitBuffers.multisampleTexture;
            target.implicitBuffers = {};
        }

        if (target.sw.owns)
            delete target.sw.paintDevice;

        target.sw = {};
#else
        {
            auto it = s_rhiRenderBuffers->begin();
            while (it != s_rhiRenderBuffers->end()) {
                if (target.renderTarget == it->renderTarget) {
                    it = s_rhiRenderBuffers->erase(it);
                    break;
                }
                ++it;
            }
        }

        if (target.owns) {
            delete target.renderTarget;
            delete target.rpDesc;
            delete target.texture;
            delete target.renderBuffer;
            delete target.depthStencil;
            delete target.paintDevice;
        }

        target.renderTarget = nullptr;
        target.rpDesc = nullptr;
        target.texture = nullptr;
        target.renderBuffer = nullptr;
        target.depthStencil = nullptr;
        target.paintDevice = nullptr;
        target.owns = false;
#endif
    }

    inline void resetWindowRenderTarget() {
        cleanupWindowRenderTarget(windowRenderTarget);
        cleanupWindowRenderTarget(preserveWindowRenderTarget);
    }
};

// Copy from qquickrendertarget.cpp
static bool createRhiRenderTarget(const QRhiColorAttachment &colorAttachment,
                                  const QSize &pixelSize,
                                  int sampleCount,
                                  QRhi *rhi,
                                  QQuickWindowRenderTarget &dst,
                                  QRhiTextureRenderTarget::Flags flags = {})
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

bool createRhiRenderTarget(QRhi *rhi, const QQuickRenderTarget &source, QQuickWindowRenderTarget &dst,
                           QRhiTextureRenderTarget::Flags rtFlags = {})
{
    auto rtd = QQuickRenderTargetPrivate::get(&source);

    switch (rtd->type) {
    case QQuickRenderTargetPrivate::Type::NativeTexture: {
        const auto format = rtd->u.nativeTexture.rhiFormat == QRhiTexture::UnknownFormat ? QRhiTexture::RGBA8
                                                                                         : QRhiTexture::Format(rtd->u.nativeTexture.rhiFormat);
        const auto textureFlags = QRhiTexture::RenderTarget | QRhiTexture::Flags(
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
                               rtd->u.nativeTexture.rhiFormatFlags
#else
                               rtd->u.nativeTexture.rhiFlags
#endif
                                                                          );
        std::unique_ptr<QRhiTexture> texture(rhi->newTexture(format, rtd->pixelSize, rtd->sampleCount, textureFlags));
        texture->setName(QByteArrayLiteral("WaylibTexture"));
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layout }))
#else
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layoutOrState }))
#endif
            return false;
        QRhiColorAttachment att(texture.get());
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst, rtFlags))
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
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst, rtFlags))
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
    WRenderHelperPrivate(WRenderHelper *qq, qw_renderer *renderer)
        : WObjectPrivate(qq)
        , renderer(renderer)
    {}
    ~WRenderHelperPrivate() {
        resetRenderBuffer(false);
        cleanupRetiredRenderBuffers(true);
    }

    struct RetiredBuffers {
        QList<BufferData *> buffers;
    };

    bool shouldDeferRenderBufferCleanup() const;
    void resetRenderBuffer(bool defer);
    void cleanupRetiredRenderBuffers(bool force);
    void onBufferDestroy();
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data);

    W_DECLARE_PUBLIC(WRenderHelper)
    qw_renderer *renderer;
    QList<BufferData*> buffers;
    QList<RetiredBuffers> retiredBuffers;
    BufferData *lastBuffer = nullptr;

    QSize size;
};

bool WRenderHelperPrivate::shouldDeferRenderBufferCleanup() const
{
#ifdef ENABLE_VULKAN_RENDER
    return renderer && qw_vulkan::isRenderer(renderer);
#else
    return false;
#endif
}

void WRenderHelperPrivate::resetRenderBuffer(bool defer)
{
    if (defer && !buffers.isEmpty()) {
        retiredBuffers.append({buffers});
    } else {
        qDeleteAll(buffers);
    }
    lastBuffer = nullptr;
    buffers.clear();
}

void WRenderHelperPrivate::cleanupRetiredRenderBuffers(bool force)
{
    Q_UNUSED(force);
    for (auto &retired : retiredBuffers)
        qDeleteAll(retired.buffers);
    retiredBuffers.clear();
}

void WRenderHelperPrivate::onBufferDestroy()
{
    qw_buffer *buffer = qobject_cast<qw_buffer*>(q_func()->sender());

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

    if (rhi->backend() == QRhi::Vulkan) {
        auto rtd = QQuickRenderTargetPrivate::get(&tmp);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        auto colorTexture = data->windowRenderTarget.res.texture;
#else
        auto colorTexture = data->windowRenderTarget.texture;
#endif
        if (!colorTexture) {
            qCWarning(lcWlRenderHelper) << "Failed to build Vulkan preserve render target: missing shared QRhi texture";
            return false;
        }

        QRhiColorAttachment colorAttachment(colorTexture);
        ok = createRhiRenderTarget(colorAttachment,
                                   rtd->pixelSize,
                                   rtd->sampleCount,
                                   rhi,
                                   data->preserveWindowRenderTarget,
                                   QRhiTextureRenderTarget::PreserveColorContents);
        if (!ok) {
            qCWarning(lcWlRenderHelper) << "Failed to build Vulkan preserve render target for QQuickRenderTarget";
            return false;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        data->preserveRenderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->preserveWindowRenderTarget.rt.renderTarget);
#else
        data->preserveRenderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->preserveWindowRenderTarget.renderTarget);
#endif
        data->preserveRenderTarget.setDevicePixelRatio(tmp.devicePixelRatio());
        data->preserveRenderTarget.setMirrorVertically(tmp.mirrorVertically());
    }

    return true;
}

WRenderHelper::WRenderHelper(qw_renderer *renderer, QObject *parent)
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
    d->resetRenderBuffer(d->shouldDeferRenderBufferCleanup());

    Q_EMIT sizeChanged();
}

void WRenderHelper::cleanupRetiredRenderResources(bool force)
{
    W_D(WRenderHelper);
    d->cleanupRetiredRenderBuffers(force);
}

bool WRenderHelper::acquireRenderBuffer(QQuickRenderControl *rc, qw_buffer *buffer, const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    W_D(WRenderHelper);
    if (!d->renderer || !qw_vulkan::isRenderer(d->renderer))
        return true;

    wlr_vk_image_attribs attribs = {};
    if (!getVulkanRenderBufferAttribs(d->renderer, buffer, purpose, &attribs))
        return false;

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed: missing Vulkan QRhi"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    if (rc->rhi()->isDeviceLost() || !rc->rhi()->isRecordingFrame()) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed: QRhi frame is not usable"
                                    << "purpose" << purpose
                                    << "deviceLost" << rc->rhi()->isDeviceLost()
                                    << "recordingFrame" << rc->rhi()->isRecordingFrame()
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    auto commandBuffer = rc->commandBuffer();
    if (!commandBuffer) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed: missing QRhi command buffer"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    commandBuffer->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(commandBuffer->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        commandBuffer->endExternal();
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed: missing native Vulkan command buffer"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    const bool ok = qw_vulkan::recordRenderBufferAcquire(d->renderer,
                                                         buffer,
                                                         handles->commandBuffer);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    qCDebug(lcWlRenderHelper) << "Vulkan render buffer acquire recorded"
                              << "purpose" << purpose
                              << "qwBuffer" << buffer
                              << "wlrBuffer" << buffer->handle()
                              << "image" << vkImageName(attribs.image)
                              << "layout" << vkImageLayoutName(attribs.layout)
                              << "format" << hex32(attribs.format)
                              << "size" << wlrBufferSize(buffer);
#else
    Q_UNUSED(rc);
    Q_UNUSED(buffer);
    Q_UNUSED(purpose);
#endif
    return true;
}

bool WRenderHelper::releaseRenderBuffer(QQuickRenderControl *rc,
                                        qw_buffer *buffer,
                                        QRhiTexture *renderTargetTexture,
                                        const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    W_D(WRenderHelper);
    if (!d->renderer || !qw_vulkan::isRenderer(d->renderer))
        return true;

    wlr_vk_image_attribs attribs = {};
    if (!getVulkanRenderBufferAttribs(d->renderer, buffer, purpose, &attribs))
        return false;

    if (!renderTargetTexture) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing Qt render target texture"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing Vulkan QRhi"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    if (rc->rhi()->isDeviceLost() || !rc->rhi()->isRecordingFrame()) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: QRhi frame is not usable"
                                    << "purpose" << purpose
                                    << "deviceLost" << rc->rhi()->isDeviceLost()
                                    << "recordingFrame" << rc->rhi()->isRecordingFrame()
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    auto commandBuffer = rc->commandBuffer();
    if (!commandBuffer) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing QRhi command buffer"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    const auto nativeTexture = renderTargetTexture->nativeTexture();
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    const auto oldLayout = VkImageLayout(nativeTexture.layout);
#else
    const auto oldLayout = VkImageLayout(nativeTexture.layout);
#endif
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: Qt render target layout is undefined"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "wlrootsLayout" << vkImageLayoutName(attribs.layout)
                                    << "oldLayout" << vkImageLayoutName(oldLayout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    commandBuffer->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(commandBuffer->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        commandBuffer->endExternal();
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing native Vulkan command buffer"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "wlrootsLayout" << vkImageLayoutName(attribs.layout)
                                    << "oldLayout" << vkImageLayoutName(oldLayout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    const bool ok = qw_vulkan::recordRenderBufferRelease(d->renderer,
                                                         buffer,
                                                         handles->commandBuffer,
                                                         oldLayout);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed"
                                    << "purpose" << purpose
                                    << "qwBuffer" << buffer
                                    << "wlrBuffer" << buffer->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "wlrootsLayout" << vkImageLayoutName(attribs.layout)
                                    << "oldLayout" << vkImageLayoutName(oldLayout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    // wlroots has recorded a transition and ownership release to GENERAL.
    // Keep QRhi's external-image tracker in sync so that a reused output
    // buffer starts from the layout which will exist after this submission.
    renderTargetTexture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);

    qCDebug(lcWlRenderHelper) << "Vulkan render buffer release recorded"
                              << "purpose" << purpose
                              << "qwBuffer" << buffer
                              << "wlrBuffer" << buffer->handle()
                              << "image" << vkImageName(attribs.image)
                              << "wlrootsLayout" << vkImageLayoutName(attribs.layout)
                              << "oldLayout" << vkImageLayoutName(oldLayout)
                              << "format" << hex32(attribs.format)
                              << "size" << wlrBufferSize(buffer);
#else
    Q_UNUSED(rc);
    Q_UNUSED(buffer);
    Q_UNUSED(renderTargetTexture);
    Q_UNUSED(purpose);
#endif
    return true;
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

class Q_DECL_HIDDEN GLTextureBuffer : public qw_buffer_interface
{
public:
    explicit GLTextureBuffer(wlr_egl *egl, QSGTexture *texture);

    QW_INTERFACE(get_dmabuf, bool, wlr_dmabuf_attributes *attribs);

private:
    wlr_egl *m_egl;
    QSGTexture *m_texture;
};

GLTextureBuffer::GLTextureBuffer(wlr_egl *egl, QSGTexture *texture)
    : m_egl(egl)
    , m_texture(texture)
{

}

bool GLTextureBuffer::get_dmabuf(wlr_dmabuf_attributes *attribs)
{
    auto rhiTexture = m_texture->rhiTexture();
    if (!rhiTexture)
        return false;

    auto display = wlr_egl_get_display(m_egl);
    auto context = wlr_egl_get_context(m_egl);

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

    attribs->width = handle()->width;
    attribs->height = handle()->height;

    return true;
}

#ifdef ENABLE_VULKAN_RENDER
class Q_DECL_HIDDEN VkTextureBuffer : public qw_buffer_interface
{
public:
    explicit VkTextureBuffer(VkInstance instance, VkDevice device, QSGTexture *texture);

    QW_INTERFACE(get_dmabuf, bool ,wlr_dmabuf_attributes *attribs);

private:
    [[maybe_unused]] VkInstance m_instance;
    [[maybe_unused]] VkDevice m_device;
    [[maybe_unused]] QSGTexture *m_texture;
};

VkTextureBuffer::VkTextureBuffer(VkInstance instance, VkDevice device, QSGTexture *texture)
    : m_instance(instance)
    , m_device(device)
    , m_texture(texture)
{

}

bool VkTextureBuffer::get_dmabuf([[maybe_unused]] wlr_dmabuf_attributes *attribs)
{
    // TODO
    return false;
}
#endif

class Q_DECL_HIDDEN QImageBuffer : public qw_buffer_interface
{
public:
    explicit QImageBuffer(const QImage &image);

    QW_INTERFACE(get_shm, bool, wlr_shm_attributes *attribs);
    QW_INTERFACE(begin_data_ptr_access, bool, uint32_t flags, void **data, uint32_t *format, size_t *stride);
    QW_INTERFACE(end_data_ptr_access, void);

private:
    QImage m_image;
};

QImageBuffer::QImageBuffer(const QImage &image)
    : m_image(image)
{

}

bool QImageBuffer::get_shm(wlr_shm_attributes *attribs)
{
    attribs->fd = 0;
    attribs->format = WTools::toDrmFormat(m_image.format());
    attribs->width = m_image.width();
    attribs->height = m_image.height();
    attribs->stride = m_image.bytesPerLine();
    return true;
}

bool QImageBuffer::begin_data_ptr_access([[maybe_unused]] uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
    *data = m_image.bits();
    *format = WTools::toDrmFormat(m_image.format());
    *stride = m_image.bytesPerLine();

    return true;
}

void QImageBuffer::end_data_ptr_access()
{

}

qw_buffer *WRenderHelper::toBuffer(qw_renderer *renderer, QSGTexture *texture, QSGRendererInterface::GraphicsApi api)
{
    const QSize size = texture->textureSize();

    switch (api) {
    case QSGRendererInterface::OpenGL: {
        Q_ASSERT(wlr_renderer_is_gles2(renderer->handle()));
        auto egl = wlr_gles2_renderer_get_egl(renderer->handle());

        return qw_buffer::create(new GLTextureBuffer(egl, texture), size.width(), size.height());
    }
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        Q_ASSERT(qw_vulkan::isRenderer(renderer));
        auto instance = qw_vulkan::rendererInstance(renderer);
        auto device = qw_vulkan::rendererDevice(renderer);

        return qw_buffer::create(new VkTextureBuffer(instance, device, texture), size.width(), size.height());
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

        return qw_buffer::create(new QImageBuffer(image), image.width(), image.height());
    }
    default:
        qFatal("Can't get qw_buffer from QSGTexture, Not supported graphics API.");
        break;
    }

    return nullptr;
}

QQuickRenderTarget WRenderHelper::acquireRenderTarget(QQuickRenderControl *rc, qw_buffer *buffer)
{
    W_D(WRenderHelper);
    Q_ASSERT(buffer);

    if (d->size.isEmpty())
        return {};

    for (int i = 0; i < d->buffers.count(); ++i) {
        auto data = d->buffers[i];
        if (data->buffer == buffer) {
            d->lastBuffer = data;
            if (!acquireRenderBuffer(rc, buffer, "cached-compositor-render-target"))
                return {};
            return data->renderTarget;
        }
    }

    std::unique_ptr<BufferData> bufferData(new BufferData);
    bufferData->buffer = buffer;
    bool importAsTexture = true;
#ifdef ENABLE_VULKAN_RENDER
    if (qw_vulkan::isRenderer(d->renderer))
        importAsTexture = false;
#endif
    qw_texture *texture = importAsTexture ? qw_texture::from_buffer(*d->renderer, *buffer) : nullptr;

    QQuickRenderTarget rt;
    bool needsVulkanRenderBufferAcquire = false;

    if (wlr_renderer_is_pixman(d->renderer->handle())) {
        Q_ASSERT(texture);
        pixman_image_t *image = wlr_pixman_texture_get_image(texture->handle());
        void *data = pixman_image_get_data(image);
        if (bufferData->paintDevice.constBits() != data)
            bufferData->paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!bufferData->paintDevice.isNull());
        rt = QQuickRenderTarget::fromPaintDevice(&bufferData->paintDevice);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (qw_vulkan::isRenderer(d->renderer)) {
        wlr_vk_image_attribs attribs = {};
        if (getVulkanRenderBufferAttribs(d->renderer, buffer, "new-compositor-render-target", &attribs)) {
            rt = QQuickRenderTarget::fromVulkanImage(attribs.image,
                                                     attribs.layout,
                                                     attribs.format,
                                                     wlrBufferSize(buffer));
            needsVulkanRenderBufferAcquire = true;
            qCDebug(lcWlRenderHelper) << "Created Qt Vulkan render target from wlroots render buffer"
                                      << "purpose" << "new-compositor-render-target"
                                      << "qwBuffer" << buffer
                                      << "wlrBuffer" << buffer->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrBufferSize(buffer);
        }
    }
#endif
    else if (wlr_renderer_is_gles2(d->renderer->handle())) {
        Q_ASSERT(texture);
        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(texture->handle(), &attribs);

        rt = QQuickRenderTarget::fromOpenGLTexture(attribs.tex, d->size);
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

        if (bufferData->renderTarget.isNull())
            return {};

        if (needsVulkanRenderBufferAcquire
            && !acquireRenderBuffer(rc, buffer, "new-compositor-render-target")) {
            return {};
        }

        if (auto texture = bufferData->windowRenderTarget.res.texture) {
            s_rhiRenderBuffers->append({ bufferData->windowRenderTarget.rt.renderTarget,
                                         texture, bufferData->buffer });
            if (bufferData->preserveWindowRenderTarget.rt.renderTarget) {
                s_rhiRenderBuffers->append({ bufferData->preserveWindowRenderTarget.rt.renderTarget,
                                             texture, bufferData->buffer });
            }
        }
    }

    connect(buffer, SIGNAL(before_destroy()),
            this, SLOT(onBufferDestroy()), Qt::UniqueConnection);

    d->buffers.append(bufferData.release());
    d->lastBuffer = d->buffers.last();

    return d->buffers.last()->renderTarget;
}

QQuickRenderTarget WRenderHelper::preserveRenderTarget(qw_buffer *buffer) const
{
    W_DC(WRenderHelper);
    for (auto data : std::as_const(d->buffers)) {
        if (data->buffer == buffer)
            return data->preserveRenderTarget;
    }

    return {};
}

std::pair<qw_buffer *, QQuickRenderTarget> WRenderHelper::lastRenderTarget() const
{
    W_DC(WRenderHelper);
    if (!d->lastBuffer)
        return {nullptr, {}};

    return {d->lastBuffer->buffer, d->lastBuffer->renderTarget};
}

static qw_renderer *createRendererWithType(const char *type, qw_backend *backend)
{
    qputenv("WLR_RENDERER", type);
    auto render = qw_renderer::autocreate(*backend);
    qunsetenv("WLR_RENDERER");

    return render;
}

qw_renderer *WRenderHelper::createRenderer(qw_backend *backend)
{
    auto api = getGraphicsApi();
    return createRenderer(backend, api);
}

qw_renderer *WRenderHelper::createRenderer(qw_backend *backend, QSGRendererInterface::GraphicsApi api)
{
    qw_renderer *renderer = nullptr;
    switch (api) {
    case QSGRendererInterface::OpenGL:
        renderer = createRendererWithType("gles2", backend);
        Q_ASSERT(!renderer || wlr_renderer_is_gles2(renderer->handle()));
        break;
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        renderer = createRendererWithType("vulkan", backend);
        Q_ASSERT(!renderer || qw_vulkan::isRenderer(renderer));
        break;
    }
#endif
    case QSGRendererInterface::Software:
        renderer = createRendererWithType("pixman", backend);
        Q_ASSERT(!renderer || wlr_renderer_is_pixman(renderer->handle()));
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

void WRenderHelper::setupRendererBackend(qw_backend *testBackend)
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
        std::unique_ptr<qw_display> display { nullptr };
        if (!testBackend) {
            display.reset(new qw_display());
            testBackend = qw_backend::autocreate(display->get_event_loop(), nullptr);

            if (!testBackend)
                qFatal("Failed to create wlr_backend");

            testBackend->start();
        }
        QQuickWindow::setGraphicsApi(WRenderHelper::probe(testBackend, apiList));
    } else if (wlrRenderer == "gles2") {
        qCInfo(lcWlRenderHelper) << "Using explicit WLR_RENDERER=gles2 with Qt OpenGL scene graph";
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    } else if (wlrRenderer == "vulkan") {
#ifdef ENABLE_VULKAN_RENDER
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
#else
        qCCritical(lcWlRenderHelper) << "WLR_RENDERER=vulkan was requested, but Vulkan support is not enabled";
        qFatal("Vulkan support is not enabled");
#endif
    } else if (wlrRenderer == "pixman") {
        qCInfo(lcWlRenderHelper) << "Using explicit WLR_RENDERER=pixman with Qt software scene graph";
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    } else {
        qCCritical(lcWlRenderHelper) << "Unknown or unsupported WLR_RENDERER:" << wlrRenderer;
        qFatal() << "Unknown/Unsupported wlr renderer: " << wlrRenderer;
    }
}

QSGRendererInterface::GraphicsApi WRenderHelper::probe(qw_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList)
{
    auto acceptApi = QSGRendererInterface::Unknown;

    for (auto api : std::as_const(apiList)) {
        std::unique_ptr<qw_renderer> renderer(createRenderer(testBackend, api));
        if (!renderer) {
            qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api failed to create wlr_renderer";
            continue;
        }

        const wlr_drm_format_set *formats = wlr_renderer_get_texture_formats(*renderer, WLR_BUFFER_CAP_DMABUF);

        if (formats && formats->len == 0) {
            qCInfo(lcWlRenderHelper) << GraphicsApiName(api) << " api don't support any format";
            continue;
        }

        // TODO: how to test when formats gets NULL
        if (formats && formats->len) {
            std::unique_ptr<qw_allocator> alloc(qw_allocator::autocreate(*testBackend, *renderer.get()));

            bool hasSupportedFormat = false;
            for (size_t formatId = 0; formatId < formats->len; formatId++) {
                auto *format = &formats->formats[formatId];

                std::unique_ptr<qw_swapchain> swapchain(qw_swapchain::create(*alloc.get(), 1000, 800, format));
                auto wbuffer = swapchain->acquire();
                if (!wbuffer) {
                    continue;
                } else {
                    std::unique_ptr<qw_buffer, qw_buffer::unlocker> buffer(qw_buffer::from(wbuffer));
                    std::unique_ptr<qw_texture> texture { qw_texture::from_buffer(*renderer.get(), *buffer.get()) };
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

static bool updateGLTexture(QRhi *rhi, qw_texture *handle, QSGPlainTexture *texture, bool) {
    wlr_gles2_texture_attribs attribs;
    wlr_gles2_texture_get_attribs(handle->handle(), &attribs);
    QSize size(handle->handle()->width, handle->handle()->height);

#define GL_TEXTURE_EXTERNAL_OES           0x8D65
    QQuickWindowPrivate::TextureFromNativeTextureFlags flags = attribs.target == GL_TEXTURE_EXTERNAL_OES
                                                                   ? QQuickWindowPrivate::NativeTextureIsExternalOES
                                                                   : QQuickWindowPrivate::TextureFromNativeTextureFlags {};
    texture->setTextureFromNativeTexture(rhi, attribs.tex, 0, 0, size, {}, flags);

    texture->setHasAlphaChannel(attribs.has_alpha);
    texture->setTextureSize(size);
    return true;
}

static inline quint64 vkimage_cast(void *image) {
    return reinterpret_cast<quintptr>(image);
}

[[maybe_unused]] static inline quint64 vkimage_cast(quint64 image) {
    return image;
}

#ifdef ENABLE_VULKAN_RENDER
static bool updateVKTexture(QRhi *rhi, qw_texture *handle, QSGPlainTexture *texture,
                            bool forceShaderReadOnlyLayout)
{
    if (!rhi || rhi->backend() != QRhi::Vulkan)
        return false;

    wlr_vk_image_attribs attribs = {};
    qw_vulkan::textureImageAttribs(handle, &attribs);
    const QSize size(handle->handle()->width, handle->handle()->height);
    const VkImageLayout qtSampleLayout = forceShaderReadOnlyLayout
                                         ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         : attribs.layout;

    QRhiTexture::Flags mappedFlags {};
    const auto mappedFormat = QSGRhiSupport::instance()->toRhiTextureFormat(attribs.format,
                                                                            &mappedFlags);
    // Qt maps both packed 10-bit channel orders to QRhi::RGB10A2, but its
    // Vulkan backend always creates an A2B10G10R10 view first. An A2R10 image
    // without MUTABLE_FORMAT would fail before we can replace that view.
    const bool requiresInexactQtView =
        attribs.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    if (mappedFormat == QRhiTexture::UnknownFormat || requiresInexactQtView) {
        qCDebug(lcWlQtQuickTexture) << "Rejected unsupported wlroots Vulkan texture format"
                                    << "wlrTexture" << handle->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "format" << hex32(attribs.format)
                                    << "reason" << (requiresInexactQtView
                                                         ? "inexact-qt-view-format"
                                                         : "unknown-qrhi-format")
                                    << "size" << size;
        return false;
    }

    const auto *nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!nativeHandles || nativeHandles->dev == VK_NULL_HANDLE || !nativeHandles->inst) {
        qCDebug(lcWlQtQuickTexture) << "Cannot create an exact Vulkan texture view without QRhi native handles"
                                    << "wlrTexture" << handle->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "format" << hex32(attribs.format);
        return false;
    }

    auto *deviceFunctions = nativeHandles->inst->deviceFunctions(nativeHandles->dev);
    if (!deviceFunctions)
        return false;

    std::unique_ptr<QRhiTexture> candidate(rhi->newTexture(mappedFormat,
                                                           size,
                                                           1,
                                                           mappedFlags));
    if (!candidate)
        return false;

    auto *vkTexture = static_cast<QVkTexture *>(candidate.get());
    // createFrom() assigns an external VkImage before it creates the default
    // image view. Keep ownership false even on a partial failure so Qt can
    // never enqueue the wlroots-owned image for destruction.
    vkTexture->owns = false;
    if (!candidate->createFrom({vkimage_cast(attribs.image), qtSampleLayout})) {
        qCDebug(lcWlQtQuickTexture) << "Failed to wrap wlroots Vulkan image in QRhiTexture"
                                    << "wlrTexture" << handle->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "format" << hex32(attribs.format)
                                    << "layout" << vkImageLayoutName(qtSampleLayout)
                                    << "size" << size;
        return false;
    }

    const auto discardUnusedCandidate = [vkTexture, nativeHandles, deviceFunctions] {
        if (vkTexture->imageView != VK_NULL_HANDLE) {
            deviceFunctions->vkDestroyImageView(nativeHandles->dev,
                                                vkTexture->imageView,
                                                nullptr);
            vkTexture->imageView = VK_NULL_HANDLE;
        }
        // The candidate has never entered a descriptor. Unregister it from
        // QRhi after synchronously dropping its unused view, while preserving
        // ownership of the external image in wlroots.
        vkTexture->destroy();
    };

    if (vkTexture->imageView == VK_NULL_HANDLE || vkTexture->lastActiveFrameSlot != -1) {
        qCDebug(lcWlQtQuickTexture) << "Cannot replace the initial Qt Vulkan texture view"
                                    << "wlrTexture" << handle->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "format" << hex32(attribs.format)
                                    << "hasInitialView" << bool(vkTexture->imageView != VK_NULL_HANDLE)
                                    << "lastActiveFrameSlot" << vkTexture->lastActiveFrameSlot;
        discardUnusedCandidate();
        return false;
    }

    const bool hasAlpha = qw_vulkan::textureHasAlpha(handle);
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = attribs.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    // Do not use QVkTexture::viewFormatForSampling here. Qt maps both
    // A2R10G10B10 and A2B10G10R10 to one QRhi format, so only the wlroots
    // format preserves the external image's real channel layout.
    viewInfo.format = attribs.format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = hasAlpha ? VK_COMPONENT_SWIZZLE_IDENTITY
                                     : VK_COMPONENT_SWIZZLE_ONE;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView samplingView = VK_NULL_HANDLE;
    const VkResult viewResult = deviceFunctions->vkCreateImageView(nativeHandles->dev,
                                                                   &viewInfo,
                                                                   nullptr,
                                                                   &samplingView);
    if (viewResult != VK_SUCCESS) {
        qCDebug(lcWlQtQuickTexture) << "Failed to create exact Vulkan sampling view"
                                    << "wlrTexture" << handle->handle()
                                    << "image" << vkImageName(attribs.image)
                                    << "format" << hex32(attribs.format)
                                    << "hasAlpha" << hasAlpha
                                    << "vkResult" << viewResult;
        discardUnusedCandidate();
        return false;
    }

    const VkImageView initialView = vkTexture->imageView;
    vkTexture->imageView = samplingView;
    vkTexture->viewFormatForSampling = attribs.format;
    deviceFunctions->vkDestroyImageView(nativeHandles->dev, initialView, nullptr);

    const char *bridgeClass = hasAlpha ? "exact-alpha-view" : "exact-opaque-view";

    WVulkanTrace::qtWrap(true, handle, vkImageValue(attribs.image), attribs.format,
                         static_cast<int>(mappedFormat), bridgeClass,
                         vkImageLayoutName(qtSampleLayout));
    texture->setTexture(candidate.release());
    WVulkanTrace::qtWrap(false, handle, vkImageValue(attribs.image), attribs.format,
                         static_cast<int>(mappedFormat), bridgeClass,
                         vkImageLayoutName(qtSampleLayout));
    texture->setHasAlphaChannel(hasAlpha);
    texture->setTextureSize(size);
    qCDebug(lcWlQtQuickTexture) << "Updated Qt Vulkan texture from wlroots texture"
                                << "wlrTexture" << handle->handle()
                                << "image" << vkImageName(attribs.image)
                                << "wlrootsLayout" << vkImageLayoutName(attribs.layout)
                                << "qtSampleLayout" << vkImageLayoutName(qtSampleLayout)
                                << "layoutPolicy" << (forceShaderReadOnlyLayout ? "shader-read-only" : "wlroots-raw")
                                << "format" << hex32(attribs.format)
                                << "viewClass" << bridgeClass
                                << "size" << size;
    return true;
}
#endif

static bool updateImage(QRhi *, qw_texture *handle, QSGPlainTexture *texture, bool) {
    auto image = wlr_pixman_texture_get_image(handle->handle());
    texture->setImage(WTools::fromPixmanImage(image));
    return true;
}

typedef bool(*UpdateTextureFunction)(QRhi *, qw_texture *, QSGPlainTexture *, bool);

static UpdateTextureFunction getUpdateTextFunction(qw_texture *handle)
{
    const auto api = WRenderHelper::getGraphicsApi();
    if (api == QSGRendererInterface::OpenGL) {
        if (!wlr_texture_is_gles2(handle->handle()))
            return nullptr;
        return updateGLTexture;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (api == QSGRendererInterface::Vulkan) {
        if (!qw_vulkan::isTexture(handle))
            return nullptr;
        return updateVKTexture;
    }
#endif
    else if (api == QSGRendererInterface::Software) {
        if (!wlr_texture_is_pixman(handle->handle()))
            return nullptr;
        return updateImage;
    }

    return nullptr;
}

bool WRenderHelper::makeTexture(QRhi *rhi, qw_texture *handle, QSGPlainTexture *texture,
                                bool forceVulkanShaderReadOnlyLayout)
{
    auto updateTexture = getUpdateTextFunction(handle);
    if (Q_UNLIKELY(!updateTexture))
        return false;
    return updateTexture(rhi, handle, texture, forceVulkanShaderReadOnlyLayout);
}

bool WRenderHelper::prepareTextureForSampling(QQuickRenderControl *rc,
                                              qw_renderer *renderer,
                                              qw_texture *texture,
                                              const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !texture)
        return true;

    if (!qw_vulkan::isRenderer(renderer))
        return true;

    if (!qw_vulkan::isTexture(texture)) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: non-Vulkan wlroots texture"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle();
        return false;
    }

    wlr_vk_image_attribs rawAttribs = {};
    qw_vulkan::textureImageAttribs(texture, &rawAttribs);

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: missing Vulkan QRhi"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(rawAttribs.image)
                                      << "wlrootsLayout" << vkImageLayoutName(rawAttribs.layout)
                                      << "format" << hex32(rawAttribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    if (rc->rhi()->isDeviceLost() || !rc->rhi()->isRecordingFrame()) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: QRhi frame is not usable"
                                      << "purpose" << purpose
                                      << "deviceLost" << rc->rhi()->isDeviceLost()
                                      << "recordingFrame" << rc->rhi()->isRecordingFrame()
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(rawAttribs.image)
                                      << "wlrootsLayout" << vkImageLayoutName(rawAttribs.layout)
                                      << "format" << hex32(rawAttribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    auto commandBuffer = rc->commandBuffer();
    if (!commandBuffer) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: missing QRhi command buffer"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(rawAttribs.image)
                                      << "wlrootsLayout" << vkImageLayoutName(rawAttribs.layout)
                                      << "format" << hex32(rawAttribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    commandBuffer->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(commandBuffer->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        commandBuffer->endExternal();
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: missing native Vulkan command buffer"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(rawAttribs.image)
                                      << "wlrootsLayout" << vkImageLayoutName(rawAttribs.layout)
                                      << "format" << hex32(rawAttribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    const bool ok = qw_vulkan::prepareTextureForSampling(renderer,
                                                         texture,
                                                         handles->commandBuffer,
                                                         nullptr);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(rawAttribs.image)
                                      << "wlrootsLayout" << vkImageLayoutName(rawAttribs.layout)
                                      << "format" << hex32(rawAttribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

#else
    Q_UNUSED(rc);
    Q_UNUSED(renderer);
    Q_UNUSED(texture);
    Q_UNUSED(purpose);
#endif
    return true;
}

bool WRenderHelper::finishTextureSampling(QQuickRenderControl *rc,
                                          qw_renderer *renderer,
                                          qw_texture *texture,
                                          const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !texture)
        return true;

    if (!qw_vulkan::isRenderer(renderer))
        return true;

    if (!qw_vulkan::isTexture(texture)) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: non-Vulkan wlroots texture"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle();
        return false;
    }

    wlr_vk_image_attribs attribs = {};
    qw_vulkan::textureImageAttribs(texture, &attribs);

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: missing Vulkan QRhi"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    if (rc->rhi()->isDeviceLost() || !rc->rhi()->isRecordingFrame()) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: QRhi frame is not usable"
                                      << "purpose" << purpose
                                      << "deviceLost" << rc->rhi()->isDeviceLost()
                                      << "recordingFrame" << rc->rhi()->isRecordingFrame()
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    auto commandBuffer = rc->commandBuffer();
    if (!commandBuffer) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: missing QRhi command buffer"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    commandBuffer->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(commandBuffer->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        commandBuffer->endExternal();
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: missing native Vulkan command buffer"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    const bool ok = qw_vulkan::finishTextureSampling(renderer,
                                                     texture,
                                                     handles->commandBuffer);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

#else
    Q_UNUSED(rc);
    Q_UNUSED(renderer);
    Q_UNUSED(texture);
    Q_UNUSED(purpose);
#endif
    return true;
}

WRenderHelper::TextureEntry
WRenderHelper::newTexture(qw_allocator *allocator, qw_renderer *renderer,
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

    wlr_buffer *buffer = allocator->create_buffer(size.width(), size.height(), &format);
    if (!buffer) {
        qCCritical(lcWlRenderHelper) << "Failed to create qw_buffer from allocator";
        return {};
    }

    std::unique_ptr<qw_texture> texture(qw_texture::from_buffer(*renderer, buffer));
    if (!texture) {
        qCCritical(lcWlRenderHelper) << "Failed to create qw_texture from buffer";
        wlr_buffer_drop(buffer);
        return {};
    }

    const auto qformat = static_cast<QRhiTexture::Format>(rhiFormat);
    const auto qflags = QRhiTexture::Flags(rhiFlags);
    std::unique_ptr<QRhiTexture> rhiTexture(rhi->newTexture(qformat, size, 1, qflags));

    if (wlr_texture_is_gles2(*texture.get())) {
        if (rhi->backend() != QRhi::OpenGLES2) {
            qFatal("The current QRhi backend doesn't support creating texture from GLES2 texture");
        }

        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(*texture.get(), &attribs);

        if (!rhiTexture->createFrom({attribs.tex, 0})) {
            qCCritical(lcWlRenderHelper, "Failed to create QRhiTexture from GLES2 texture");
            wlr_buffer_drop(buffer);
            return {};
        }
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (qw_vulkan::isTexture(texture.get())) {
        if (rhi->backend() != QRhi::Vulkan) {
            qFatal("The current QRhi backend doesn't support creating texture from Vulkan image");
        }

        wlr_vk_image_attribs attribs = {};
        if (!qw_vulkan::renderBufferAttribs(renderer, buffer, &attribs)) {
            qCWarning(lcWlRenderHelper) << "Failed to create QRhiTexture from wlroots Vulkan render buffer attributes"
                                        << "purpose" << "internal-rhi-texture"
                                        << "wlrBuffer" << buffer
                                        << "size" << size;
            wlr_buffer_drop(buffer);
            return {};
        }

        if (!rhiTexture->createFrom({vkimage_cast(attribs.image),
                                     attribs.layout})) {
            qCCritical(lcWlRenderHelper, "Failed to create QRhiTexture from Vulkan image");
            wlr_buffer_drop(buffer);
            return {};
        }
    }
#endif
    else if (wlr_texture_is_pixman(*texture.get())) {
        qFatal("Creating QRhiTexture from Pixman image is not supported");
    } else {
        qFatal("Unknown texture type");
    }

    rhiTexture->setName("WaylibTexture");

    return {buffer, texture.release(), rhiTexture.release()};
}

WRenderHelper::TextureEntry
WRenderHelper::newTextureLike(QW_NAMESPACE::qw_allocator *allocator,
                              QW_NAMESPACE::qw_renderer *renderer,
                              QRhiTexture *texture, QRhi *rhi,
                              int rhiFlags)
{
    auto buffer = lookupBuffer(texture);
    if (!buffer)
        return {};

    wlr_dmabuf_attributes attribs;
    if (!buffer->get_dmabuf(&attribs))
        return {};

    return newTexture(allocator, renderer, attribs.format, attribs.modifier,
                      rhi, texture->pixelSize(), texture->format(), rhiFlags);
}

QW_NAMESPACE::qw_buffer *WRenderHelper::lookupBuffer(const QRhiRenderTarget *rt)
{
    for (const auto &entry : std::as_const(*s_rhiRenderBuffers)) {
        if (entry.renderTarget == rt)
            return entry.buffer;
    }

    return nullptr;
}

QW_NAMESPACE::qw_buffer *WRenderHelper::lookupBuffer(const QRhiTexture *texture)
{
    for (const auto &entry : std::as_const(*s_rhiRenderBuffers)) {
        if (entry.texture == texture)
            return entry.buffer;
    }

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wrenderhelper.cpp"
