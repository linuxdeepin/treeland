// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderhelper.h"
#include "wtools.h"
#include "wayliblogging.h"
#include "private/wqmlhelper_p.h"
#include "private/wglobal_p.h"
#include "private/wprivateaccessor_p.h"
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

#include <limits>
#include <type_traits>

#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
W_DECLARE_PRIVATE_MEMBER(QRhi_d_tag, QRhi, d, QRhiImplementation *);
#endif

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
                                  << "usage" << hex32(attribs->usage)
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

#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
struct Q_DECL_HIDDEN VulkanDepthStencilAllocation {
    QRhi *rhi = nullptr;
    QVulkanDeviceFunctions *deviceFunctions = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    bool registeredWithRhi = false;

    ~VulkanDepthStencilAllocation()
    {
        release();
    }

    void release()
    {
        if (image == VK_NULL_HANDLE && memory == VK_NULL_HANDLE)
            return;

        if (registeredWithRhi) {
            // QRhi defers framebuffer and image-view destruction. Flush that
            // queue before releasing the attachment they reference. This path
            // only runs when render targets are retired, never per frame.
            if (!rhi || rhi->finish() != QRhi::FrameOpSuccess) {
                qCWarning(lcWlRenderHelper)
                    << "Keeping native Vulkan depth-stencil allocation alive after QRhi finish failed"
                    << "image" << vkImageName(image)
                    << "format" << hex32(format);
                image = VK_NULL_HANDLE;
                memory = VK_NULL_HANDLE;
                return;
            }
        }

        if (deviceFunctions && device != VK_NULL_HANDLE) {
            if (image != VK_NULL_HANDLE)
                deviceFunctions->vkDestroyImage(device, image, nullptr);
            if (memory != VK_NULL_HANDLE)
                deviceFunctions->vkFreeMemory(device, memory, nullptr);
        }

        if (WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderHelper).noquote()
                << QStringLiteral("VKTRACE event=blitter-depth-destroy image=%1 format=%2")
                       .arg(vkImageValue(image), 0, 16)
                       .arg(quint32(format), 0, 16);
        }

        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

static bool findVulkanDepthStencilMemoryType(
    const VkPhysicalDeviceMemoryProperties &properties,
    uint32_t compatibleTypes,
    uint32_t *memoryTypeIndex,
    VkMemoryPropertyFlags *memoryTypeFlags)
{
    const auto find = [&] (VkMemoryPropertyFlags required) {
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
            if ((compatibleTypes & (uint32_t(1) << i)) == 0
                || (flags & required) != required
                || (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)) {
                continue;
            }

            *memoryTypeIndex = i;
            *memoryTypeFlags = flags;
            return true;
        }
        return false;
    };

    return find(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) || find(0);
}

static bool createNativeVulkanDepthStencilTexture(
    QRhi *rhi,
    const QSize &pixelSize,
    int sampleCount,
    std::unique_ptr<QRhiTexture> *texture,
    std::unique_ptr<VulkanDepthStencilAllocation> *allocation)
{
    if (!rhi || rhi->backend() != QRhi::Vulkan || pixelSize.isEmpty()
        || sampleCount != 1) {
        return false;
    }

    const auto *nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(
        rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->inst
        || nativeHandles->physDev == VK_NULL_HANDLE
        || nativeHandles->dev == VK_NULL_HANDLE) {
        qCWarning(lcWlRenderHelper)
            << "Cannot create attachment-only Vulkan depth-stencil texture without native handles";
        return false;
    }

    QVulkanFunctions *instanceFunctions = nativeHandles->inst->functions();
    QVulkanDeviceFunctions *deviceFunctions = nativeHandles->inst->deviceFunctions(
        nativeHandles->dev);
    if (!instanceFunctions || !deviceFunctions)
        return false;

    constexpr VkFormat candidates[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkFormatProperties formatProperties = {};
    for (VkFormat candidate : candidates) {
        instanceFunctions->vkGetPhysicalDeviceFormatProperties(
            nativeHandles->physDev, candidate, &formatProperties);
        if (!(formatProperties.optimalTilingFeatures
              & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
            continue;
        }

        VkImageFormatProperties imageFormatProperties = {};
        const VkResult formatResult =
            instanceFunctions->vkGetPhysicalDeviceImageFormatProperties(
                nativeHandles->physDev,
                candidate,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                0,
                &imageFormatProperties);
        if (formatResult != VK_SUCCESS
            || imageFormatProperties.maxExtent.width
                < uint32_t(pixelSize.width())
            || imageFormatProperties.maxExtent.height
                < uint32_t(pixelSize.height())
            || !(imageFormatProperties.sampleCounts & VK_SAMPLE_COUNT_1_BIT)) {
            continue;
        }

        format = candidate;
        break;
    }
    if (format == VK_FORMAT_UNDEFINED) {
        qCWarning(lcWlRenderHelper)
            << "No attachment-capable Vulkan depth-stencil format is available";
        return false;
    }

    auto nativeAllocation = std::make_unique<VulkanDepthStencilAllocation>();
    nativeAllocation->rhi = rhi;
    nativeAllocation->deviceFunctions = deviceFunctions;
    nativeAllocation->device = nativeHandles->dev;
    nativeAllocation->format = format;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {
        uint32_t(pixelSize.width()),
        uint32_t(pixelSize.height()),
        1,
    };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = deviceFunctions->vkCreateImage(
        nativeHandles->dev, &imageInfo, nullptr, &nativeAllocation->image);
    if (result != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Failed to create attachment-only Vulkan depth-stencil image"
            << "format" << hex32(format)
            << "size" << pixelSize
            << "vkResult" << result;
        return false;
    }

    VkMemoryRequirements memoryRequirements = {};
    deviceFunctions->vkGetImageMemoryRequirements(
        nativeHandles->dev, nativeAllocation->image, &memoryRequirements);
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    instanceFunctions->vkGetPhysicalDeviceMemoryProperties(
        nativeHandles->physDev, &memoryProperties);

    uint32_t memoryTypeIndex = std::numeric_limits<uint32_t>::max();
    VkMemoryPropertyFlags memoryTypeFlags = 0;
    if (!findVulkanDepthStencilMemoryType(memoryProperties,
                                          memoryRequirements.memoryTypeBits,
                                          &memoryTypeIndex,
                                          &memoryTypeFlags)) {
        qCWarning(lcWlRenderHelper)
            << "No non-lazy Vulkan memory type is available for depth-stencil attachment"
            << "format" << hex32(format)
            << "memoryTypeBits" << hex32(memoryRequirements.memoryTypeBits);
        return false;
    }

    VkMemoryAllocateInfo memoryInfo = {};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.allocationSize = memoryRequirements.size;
    memoryInfo.memoryTypeIndex = memoryTypeIndex;
    result = deviceFunctions->vkAllocateMemory(
        nativeHandles->dev, &memoryInfo, nullptr, &nativeAllocation->memory);
    if (result != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Failed to allocate Vulkan depth-stencil attachment memory"
            << "format" << hex32(format)
            << "allocationSize" << memoryRequirements.size
            << "memoryTypeIndex" << memoryTypeIndex
            << "vkResult" << result;
        return false;
    }

    result = deviceFunctions->vkBindImageMemory(nativeHandles->dev,
                                                 nativeAllocation->image,
                                                 nativeAllocation->memory,
                                                 0);
    if (result != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Failed to bind Vulkan depth-stencil attachment memory"
            << "format" << hex32(format)
            << "vkResult" << result;
        return false;
    }

    std::unique_ptr<QRhiTexture> depthTexture(
        rhi->newTexture(QRhiTexture::D24S8, pixelSize, sampleCount,
                        QRhiTexture::RenderTarget));
    if (!depthTexture)
        return false;

    auto *vkTexture = static_cast<QVkTexture *>(depthTexture.get());
    vkTexture->owns = false;
    vkTexture->image = nativeAllocation->image;
    vkTexture->imageView = VK_NULL_HANDLE;
    vkTexture->imageAlloc = nullptr;
    vkTexture->usageState = {
        VK_IMAGE_LAYOUT_UNDEFINED,
        0,
        0,
    };
    vkTexture->vkformat = format;
    vkTexture->viewFormat = format;
    vkTexture->viewFormatForSampling = format;
    vkTexture->mipLevelCount = 1;
    vkTexture->samples = VK_SAMPLE_COUNT_1_BIT;
    vkTexture->generation += 1;
    depthTexture->setName(
        QByteArrayLiteral("WaylibVulkanBackdropAttachmentOnlyDepthStencilTexture"));

    auto *rhiImplementation = static_cast<QRhiVulkan *>(
        W_PRIVATE_MEMBER(*rhi, QRhi_d_tag {}));
    if (!rhiImplementation)
        return false;
    rhiImplementation->registerResource(vkTexture, false);
    nativeAllocation->registeredWithRhi = true;

    if (WVulkanTrace::enabled()) {
        qCDebug(lcWlRenderHelper).noquote()
            << QStringLiteral("VKTRACE event=blitter-depth-create path=attachment-only image=%1 format=%2 size=%3x%4 memoryType=%5 memoryFlags=%6")
                   .arg(vkImageValue(nativeAllocation->image), 0, 16)
                   .arg(quint32(format), 0, 16)
                   .arg(pixelSize.width())
                   .arg(pixelSize.height())
                   .arg(memoryTypeIndex)
                   .arg(quint32(memoryTypeFlags), 0, 16);
    }

    *texture = std::move(depthTexture);
    *allocation = std::move(nativeAllocation);
    return true;
}

static bool createVulkanBackdropDepthStencilTexture(
    QRhi *rhi,
    const QSize &pixelSize,
    int sampleCount,
    std::unique_ptr<QRhiTexture> *texture,
    std::unique_ptr<VulkanDepthStencilAllocation> *allocation)
{
    if (rhi->isTextureFormatSupported(QRhiTexture::D24S8)) {
        std::unique_ptr<QRhiTexture> depthTexture(
            rhi->newTexture(QRhiTexture::D24S8, pixelSize, sampleCount,
                            QRhiTexture::RenderTarget));
        if (depthTexture) {
            depthTexture->setName(
                QByteArrayLiteral("WaylibVulkanBackdropDepthStencilTexture"));
            if (depthTexture->create()) {
                if (WVulkanTrace::enabled()) {
                    qCDebug(lcWlRenderHelper).noquote()
                        << QStringLiteral("VKTRACE event=blitter-depth-create path=qrhi-d24s8 texture=%1 size=%2x%3")
                               .arg(quintptr(depthTexture.get()), 0, 16)
                               .arg(pixelSize.width())
                               .arg(pixelSize.height());
                }
                *texture = std::move(depthTexture);
                return true;
            }
        }

        qCDebug(lcWlRenderHelper)
            << "QRhi D24S8 depth-stencil texture creation failed; trying attachment-only fallback"
            << "size" << pixelSize
            << "sampleCount" << sampleCount;
    }

    return createNativeVulkanDepthStencilTexture(
        rhi, pixelSize, sampleCount, texture, allocation);
}
#endif

struct Q_DECL_HIDDEN BufferData {
    BufferData() {

    }

    ~BufferData() {
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (vulkanBackdropRhi)
            vulkanBackdropRhi->removeCleanupCallback(this);
#endif
        resetWindowRenderTarget();
    }

    qw_buffer *buffer = nullptr;
    // for software renderer
    WImageRenderTarget paintDevice;
    QQuickRenderTarget renderTarget;
    QQuickWindowRenderTarget windowRenderTarget;
    QQuickRenderTarget preserveRenderTarget;
    QQuickWindowRenderTarget preserveWindowRenderTarget;
    QQuickRenderTarget vulkanBackdropRenderTarget;
    QQuickWindowRenderTarget vulkanBackdropWindowRenderTarget;
    QQuickRenderTarget vulkanBackdropPreserveRenderTarget;
    QQuickWindowRenderTarget vulkanBackdropPreserveWindowRenderTarget;
    QQuickRenderTarget vulkanBackdropResumeRenderTarget;
    QQuickWindowRenderTarget vulkanBackdropResumeWindowRenderTarget;
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    std::unique_ptr<VulkanDepthStencilAllocation> vulkanBackdropDepthStencilAllocation;
    QRhi *vulkanBackdropRhi = nullptr;
#endif
    bool vulkanBackdropUnavailableLogged = false;

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
        // The three backdrop targets share the depth texture owned by the
        // clear target. Destroy all borrowers before destroying the owner.
        cleanupWindowRenderTarget(vulkanBackdropResumeWindowRenderTarget);
        cleanupWindowRenderTarget(vulkanBackdropPreserveWindowRenderTarget);
        cleanupWindowRenderTarget(vulkanBackdropWindowRenderTarget);
        cleanupWindowRenderTarget(preserveWindowRenderTarget);
        cleanupWindowRenderTarget(windowRenderTarget);
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        vulkanBackdropDepthStencilAllocation.reset();
#endif
    }

#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    void registerVulkanBackdropCleanup(QRhi *rhi)
    {
        if (vulkanBackdropRhi == rhi)
            return;
        if (vulkanBackdropRhi)
            vulkanBackdropRhi->removeCleanupCallback(this);

        vulkanBackdropRhi = rhi;
        if (!vulkanBackdropRhi)
            return;

        vulkanBackdropRhi->addCleanupCallback(this, [this] (QRhi *cleanupRhi) {
            if (vulkanBackdropRhi != cleanupRhi)
                return;

            // The callback list is being cleared by QRhi. Avoid trying to
            // remove this callback while it is being invoked.
            vulkanBackdropRhi = nullptr;
            resetWindowRenderTarget();
        });
    }
#endif
};

// Copy from qquickrendertarget.cpp
static bool createRhiRenderTarget(const QRhiColorAttachment &colorAttachment,
                                  const QSize &pixelSize,
                                  int sampleCount,
                                  QRhi *rhi,
                                  QQuickWindowRenderTarget &dst,
                                  QRhiTextureRenderTarget::Flags flags = {})
{
    std::unique_ptr<QRhiRenderBuffer> depthStencil(
        rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, sampleCount));
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
static bool createRhiRenderTargetWithDepthTexture(
    const QRhiColorAttachment &colorAttachment,
    const QSize &pixelSize,
    int sampleCount,
    QRhi *rhi,
    QQuickWindowRenderTarget &dst,
    QRhiTextureRenderTarget::Flags flags,
    QRhiTexture *sharedDepthStencilTexture,
    const char *name = "WaylibVulkanBackdropRenderTarget")
{
    if (!sharedDepthStencilTexture)
        return false;

    QRhiTextureRenderTargetDescription rtDesc(colorAttachment);
    rtDesc.setDepthTexture(sharedDepthStencilTexture);
    std::unique_ptr<QRhiTextureRenderTarget> rt(
        rhi->newTextureRenderTarget(rtDesc, flags));
    std::unique_ptr<QRhiRenderPassDescriptor> rp(
        rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());
    rt->setName(QByteArray(name));
    if (!rt->create()) {
        qCWarning(lcWlRenderHelper)
            << "Failed to build Vulkan backdrop texture render target"
            << "name" << name
            << "pixelSize" << pixelSize
            << "sampleCount" << sampleCount
            << "flags" << flags.toInt();
        return false;
    }

    dst.rt.renderTarget = rt.release();
    dst.res.rpDesc = rp.release();
    dst.rt.owns = true;
    return true;
}
#endif

bool createRhiRenderTarget(QRhi *rhi, const QQuickRenderTarget &source, QQuickWindowRenderTarget &dst,
                           QRhiTextureRenderTarget::Flags rtFlags = {},
                           QRhiTexture::Flags extraTextureFlags = {})
{
    auto rtd = QQuickRenderTargetPrivate::get(&source);

    switch (rtd->type) {
    case QQuickRenderTargetPrivate::Type::NativeTexture: {
        const auto format = rtd->u.nativeTexture.rhiFormat == QRhiTexture::UnknownFormat ? QRhiTexture::RGBA8
                                                                                         : QRhiTexture::Format(rtd->u.nativeTexture.rhiFormat);
        const auto textureFlags = QRhiTexture::RenderTarget | extraTextureFlags | QRhiTexture::Flags(
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
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data,
                                      QRhiTexture::Flags extraTextureFlags = {});
    bool ensureVulkanBackdropRenderTargets(QQuickRenderControl *rc,
                                           BufferData *data);

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

bool WRenderHelperPrivate::ensureRhiRenderTarget(QQuickRenderControl *rc,
                                                 BufferData *data,
                                                 QRhiTexture::Flags extraTextureFlags)
{
    data->resetWindowRenderTarget();
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    auto rhi = QQuickRenderControlPrivate::get(rc)->rhi;
#else
    auto rhi = rc->rhi();
#endif
    auto tmp = data->renderTarget;
    bool ok = createRhiRenderTarget(rhi, tmp, data->windowRenderTarget, {}, extraTextureFlags);
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
            qCWarning(lcWlRenderHelper)
                << "Failed to build Vulkan preserve render target: missing shared QRhi texture";
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

bool WRenderHelperPrivate::ensureVulkanBackdropRenderTargets(QQuickRenderControl *rc,
                                                             BufferData *data)
{
#if !defined(ENABLE_VULKAN_RENDER) || QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    Q_UNUSED(rc);
    Q_UNUSED(data);
    return false;
#else
    if (data->vulkanBackdropWindowRenderTarget.rt.renderTarget
        && data->vulkanBackdropPreserveWindowRenderTarget.rt.renderTarget
        && data->vulkanBackdropResumeWindowRenderTarget.rt.renderTarget) {
        return true;
    }
    if (data->vulkanBackdropUnavailableLogged)
        return false;

    QRhi *rhi = rc ? rc->rhi() : nullptr;
    auto colorTexture = data->windowRenderTarget.res.texture;
    const auto *nativeHandles = rhi && rhi->backend() == QRhi::Vulkan
        ? static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles())
        : nullptr;
    const bool compatibleRenderer = renderer && qw_vulkan::isRenderer(renderer)
        && qw_vulkan::rendererHasSeparateDepthStencilLayouts(renderer);
    const bool sameDevice = compatibleRenderer && nativeHandles
        && nativeHandles->physDev == qw_vulkan::rendererPhysicalDevice(renderer)
        && nativeHandles->dev == qw_vulkan::rendererDevice(renderer)
        && nativeHandles->gfxQueueFamilyIdx == qw_vulkan::rendererQueueFamily(renderer)
        && nativeHandles->gfxQueue == qw_vulkan::rendererQueue(renderer);
    if (!rhi || rhi->backend() != QRhi::Vulkan || !colorTexture
        || colorTexture->sampleCount() != 1
        || !colorTexture->flags().testFlag(QRhiTexture::UsedAsTransferSource)
        || !compatibleRenderer || !sameDevice) {
        if (!data->vulkanBackdropUnavailableLogged) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan backdrop targets are unavailable"
                << "reason" << "incompatible renderer, device, or color attachment"
                << "rhi" << rhi
                << "colorTexture" << colorTexture
                << "sampleCount" << (colorTexture ? colorTexture->sampleCount() : 0)
                << "transferSource"
                << (colorTexture
                    && colorTexture->flags().testFlag(QRhiTexture::UsedAsTransferSource))
                << "separateDepthStencilLayouts" << compatibleRenderer
                << "sameDevice" << sameDevice;
            data->vulkanBackdropUnavailableLogged = true;
        }
        return false;
    }

    BufferData::cleanupWindowRenderTarget(data->vulkanBackdropResumeWindowRenderTarget);
    BufferData::cleanupWindowRenderTarget(data->vulkanBackdropPreserveWindowRenderTarget);
    BufferData::cleanupWindowRenderTarget(data->vulkanBackdropWindowRenderTarget);
    data->vulkanBackdropRenderTarget = {};
    data->vulkanBackdropPreserveRenderTarget = {};
    data->vulkanBackdropResumeRenderTarget = {};
    data->vulkanBackdropDepthStencilAllocation.reset();

    const QSize pixelSize = colorTexture->pixelSize();
    const int sampleCount = colorTexture->sampleCount();
    const QRhiColorAttachment colorAttachment(colorTexture);
    std::unique_ptr<QRhiTexture> ownedDepthTexture;
    std::unique_ptr<VulkanDepthStencilAllocation> nativeDepthStencilAllocation;

    bool ok = createVulkanBackdropDepthStencilTexture(
        rhi, pixelSize, sampleCount, &ownedDepthTexture,
        &nativeDepthStencilAllocation);
    QRhiTexture *depthTexture = ownedDepthTexture.get();
    if (ok) {
        ok = createRhiRenderTargetWithDepthTexture(
            colorAttachment, pixelSize, sampleCount, rhi,
            data->vulkanBackdropWindowRenderTarget, {}, depthTexture,
            "WaylibVulkanBackdropClearRenderTarget");
    }
    if (ok) {
        ok = createRhiRenderTargetWithDepthTexture(
            colorAttachment, pixelSize, sampleCount, rhi,
            data->vulkanBackdropPreserveWindowRenderTarget,
            QRhiTextureRenderTarget::PreserveColorContents,
            depthTexture,
            "WaylibVulkanBackdropPreserveColorRenderTarget");
    }
    if (ok) {
        ok = createRhiRenderTargetWithDepthTexture(
            colorAttachment, pixelSize, sampleCount, rhi,
            data->vulkanBackdropResumeWindowRenderTarget,
            QRhiTextureRenderTarget::PreserveColorContents
                | QRhiTextureRenderTarget::PreserveDepthStencilContents,
            depthTexture,
            "WaylibVulkanBackdropResumeRenderTarget");
    }

    if (!ok) {
        BufferData::cleanupWindowRenderTarget(data->vulkanBackdropResumeWindowRenderTarget);
        BufferData::cleanupWindowRenderTarget(data->vulkanBackdropPreserveWindowRenderTarget);
        BufferData::cleanupWindowRenderTarget(data->vulkanBackdropWindowRenderTarget);
        ownedDepthTexture.reset();
        nativeDepthStencilAllocation.reset();
        if (!data->vulkanBackdropUnavailableLogged) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan backdrop targets are unavailable"
                << "reason" << "target creation failed"
                << "pixelSize" << pixelSize
                << "sampleCount" << sampleCount;
            data->vulkanBackdropUnavailableLogged = true;
        }
        return false;
    }

    // Keep exactly one owner for the texture while all three render targets
    // borrow it. The native allocation, when used, must outlive the wrapper.
    data->vulkanBackdropWindowRenderTarget.implicitBuffers.depthStencilTexture =
        ownedDepthTexture.release();
    data->vulkanBackdropDepthStencilAllocation =
        std::move(nativeDepthStencilAllocation);
    data->registerVulkanBackdropCleanup(rhi);

    const auto makeQuickTarget = [&] (QQuickWindowRenderTarget &windowTarget) {
        QQuickRenderTarget target = QQuickRenderTarget::fromRhiRenderTarget(
            windowTarget.rt.renderTarget);
        target.setDevicePixelRatio(data->renderTarget.devicePixelRatio());
        target.setMirrorVertically(data->renderTarget.mirrorVertically());
        return target;
    };
    data->vulkanBackdropRenderTarget = makeQuickTarget(
        data->vulkanBackdropWindowRenderTarget);
    data->vulkanBackdropPreserveRenderTarget = makeQuickTarget(
        data->vulkanBackdropPreserveWindowRenderTarget);
    data->vulkanBackdropResumeRenderTarget = makeQuickTarget(
        data->vulkanBackdropResumeWindowRenderTarget);

    s_rhiRenderBuffers->append({
        data->vulkanBackdropWindowRenderTarget.rt.renderTarget,
        colorTexture, data->buffer });
    s_rhiRenderBuffers->append({
        data->vulkanBackdropPreserveWindowRenderTarget.rt.renderTarget,
        colorTexture, data->buffer });
    s_rhiRenderBuffers->append({
        data->vulkanBackdropResumeWindowRenderTarget.rt.renderTarget,
        colorTexture, data->buffer });

    if (WVulkanTrace::enabled()) {
        qCDebug(lcWlRenderHelper).noquote()
            << QStringLiteral("VKTRACE event=blitter-targets-create buffer=%1 color=%2 depth=%3 clear=%4 preserveColor=%5 resume=%6 size=%7x%8")
                   .arg(quintptr(data->buffer), 0, 16)
                   .arg(quintptr(colorTexture), 0, 16)
                   .arg(quintptr(depthTexture), 0, 16)
                   .arg(quintptr(data->vulkanBackdropWindowRenderTarget.rt.renderTarget), 0, 16)
                   .arg(quintptr(data->vulkanBackdropPreserveWindowRenderTarget.rt.renderTarget), 0, 16)
                   .arg(quintptr(data->vulkanBackdropResumeWindowRenderTarget.rt.renderTarget), 0, 16)
                   .arg(pixelSize.width())
                   .arg(pixelSize.height());
    }

    return true;
#endif
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

QQuickRenderTarget WRenderHelper::acquireRenderTarget(QQuickRenderControl *rc,
                                                      qw_buffer *buffer,
                                                      bool useVulkanBackdrop)
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
            if (useVulkanBackdrop
                && d->ensureVulkanBackdropRenderTargets(rc, data)) {
                return data->vulkanBackdropRenderTarget;
            }
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
    QRhiTexture::Flags renderTargetTextureFlags;

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
            if (attribs.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                renderTargetTextureFlags |= QRhiTexture::UsedAsTransferSource;
            needsVulkanRenderBufferAcquire = true;
            qCDebug(lcWlRenderHelper) << "Created Qt Vulkan render target from wlroots render buffer"
                                      << "purpose" << "new-compositor-render-target"
                                      << "qwBuffer" << buffer
                                      << "wlrBuffer" << buffer->handle()
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "usage" << hex32(attribs.usage)
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
            if (!d->ensureRhiRenderTarget(rc, bufferData.get(), renderTargetTextureFlags))
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

    if (useVulkanBackdrop
        && d->ensureVulkanBackdropRenderTargets(rc, d->buffers.last())) {
        return d->buffers.last()->vulkanBackdropRenderTarget;
    }
    return d->buffers.last()->renderTarget;
}

QQuickRenderTarget WRenderHelper::preserveRenderTarget(qw_buffer *buffer,
                                                       bool useVulkanBackdrop) const
{
    W_DC(WRenderHelper);
    for (auto data : std::as_const(d->buffers)) {
        if (data->buffer == buffer) {
            return useVulkanBackdrop
                ? data->vulkanBackdropPreserveRenderTarget
                : data->preserveRenderTarget;
        }
    }

    return {};
}

QQuickRenderTarget WRenderHelper::vulkanBackdropResumeRenderTarget(qw_buffer *buffer) const
{
    W_DC(WRenderHelper);
    for (auto data : std::as_const(d->buffers)) {
        if (data->buffer == buffer)
            return data->vulkanBackdropResumeRenderTarget;
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

bool WRenderHelper::beginTextureSyncBatch(QQuickRenderControl *rc,
                                          qw_renderer *renderer,
                                          bool verifyQueue)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !qw_vulkan::isRenderer(renderer))
        return true;

    if (verifyQueue) {
        if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
            qCWarning(lcWlRenderHelper) << "Cannot begin Vulkan texture sync batch: missing Vulkan QRhi";
            return false;
        }

        const auto *nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(rc->rhi()->nativeHandles());
        if (!nativeHandles
            || nativeHandles->dev != qw_vulkan::rendererDevice(renderer)
            || nativeHandles->gfxQueueFamilyIdx != qw_vulkan::rendererQueueFamily(renderer)
            || nativeHandles->gfxQueue != qw_vulkan::rendererQueue(renderer)) {
            qCWarning(lcWlRenderHelper) << "Cannot begin Vulkan texture sync batch: Qt and wlroots do not share the same queue"
                                        << "qtDevice" << (nativeHandles ? nativeHandles->dev : VK_NULL_HANDLE)
                                        << "wlrootsDevice" << qw_vulkan::rendererDevice(renderer)
                                        << "qtQueueFamily" << (nativeHandles ? nativeHandles->gfxQueueFamilyIdx : 0)
                                        << "wlrootsQueueFamily" << qw_vulkan::rendererQueueFamily(renderer);
            return false;
        }
    }

    if (!qw_vulkan::beginTextureSyncBatch(renderer)) {
        qCWarning(lcWlRenderHelper) << "Failed to begin Vulkan texture sync batch; using immediate CPU waits for this frame";
        return false;
    }
#else
    Q_UNUSED(rc);
    Q_UNUSED(renderer);
    Q_UNUSED(verifyQueue);
#endif
    return true;
}

bool WRenderHelper::flushTextureSyncBatch(qw_renderer *renderer)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !qw_vulkan::isRenderer(renderer))
        return true;

    if (!qw_vulkan::flushTextureSyncBatch(renderer)) {
        qCWarning(lcWlRenderHelper) << "Failed to flush Vulkan texture sync batch";
        return false;
    }
#else
    Q_UNUSED(renderer);
#endif
    return true;
}

void WRenderHelper::abortTextureSyncBatch(qw_renderer *renderer)
{
#ifdef ENABLE_VULKAN_RENDER
    if (renderer && qw_vulkan::isRenderer(renderer))
        qw_vulkan::abortTextureSyncBatch(renderer);
#else
    Q_UNUSED(renderer);
#endif
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
