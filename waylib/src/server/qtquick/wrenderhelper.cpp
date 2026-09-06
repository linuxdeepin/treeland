// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderhelper.h"
#include "wtools.h"
#include "wscoplistener.h"
#include "wpointer.h"
#include "wayliblogging.h"
#include "private/wqmlhelper_p.h"
#include "private/wglobal_p.h"
#include "private/wprivateaccessor_p.h"
#include "utils/private/wvulkantrace_p.h"

#include <memory>

#include <wlr_all.h>
#include <wcontainerof.h>

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
using QRhiResourceMap = QHash<QRhiResource *, bool>;
W_DECLARE_PRIVATE_MEMBER(QRhiResources_tag, QRhiImplementation, resources, QRhiResourceMap);
#endif

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN RhiRenderEntry {
    const QRhiRenderTarget *renderTarget;
    const QRhiTexture *texture;
    wlr_buffer *buffer;
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

static QSize wlrBufferSize(wlr_buffer *buffer)
{
    return QSize(buffer->width, buffer->height);
}

static QSize wlrTextureSize(wlr_texture *texture)
{
    return QSize(texture->width, texture->height);
}

#ifdef ENABLE_VULKAN_RENDER
// wlroots-recorded barriers must never go into an active QRhi pass instance
// (see the checks inside WRenderHelper's barrier recording wrappers below).
static bool qtVulkanPassIsRecording(QRhiCommandBuffer *cb)
{
    if (!cb)
        return false;
    const auto *vkCb = static_cast<const QVkCommandBuffer *>(cb);
    return vkCb->recordingPass != QVkCommandBuffer::NoPass;
}
#endif

static bool getVulkanRenderBufferAttribs(wlr_renderer *renderer,
                                         wlr_buffer *buffer,
                                         const char *purpose,
                                         wlr_vk_image_attribs *attribs)
{
    if (!renderer || !buffer || !wlr_renderer_is_vk(renderer))
        return false;

    if (waylib_vk_renderer_get_render_buffer_attribs(renderer, buffer, attribs)) {
        qCDebug(lcWlRenderHelper) << "Got wlroots Vulkan render buffer attributes"
                                  << "purpose" << purpose
                                  << "buffer" << buffer
                                  << "image" << vkImageName(attribs->image)
                                  << "layout" << vkImageLayoutName(attribs->layout)
                                  << "format" << hex32(attribs->format)
                                  << "usage" << hex32(attribs->usage)
                                  << "size" << wlrBufferSize(buffer);
        return true;
    }

    qCWarning(lcWlRenderHelper) << "Failed to get wlroots Vulkan render buffer attributes"
                                << "purpose" << purpose
                                << "buffer" << buffer
                                << "size" << wlrBufferSize(buffer);
    return false;
}
#endif

#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)

// Qt's own leak report in ~QRhiImplementation runs after QRhiVulkan::destroy()
// has already aborted the process through the VMA assertion when any
// QRhiResource created on this QRhi outlives it (QML items or scene graph
// nodes that own QRhiBuffer/QRhiTexture objects and are torn down after the
// render control has released the QRhi). A QRhi cleanup callback still runs
// from ~QRhi before destroy(), while the VkDevice is alive, which is the
// only point where the leaked native objects can still be released safely:
// QVkBuffer/QVkTexture/QVkRenderBuffer::destroy() enqueue a deferred release
// that QRhiVulkan::destroy() force-executes (after vkDeviceWaitIdle) before
// it destroys the VmaAllocator. Qt-internal objects (command buffers, swap
// chains) and resources without VMA memory (samplers, pipelines, shader
// resource bindings, render-pass descriptors) are deliberately untouched.
static void forceReleaseVulkanGraphicsResources(QRhi *rhi)
{
    if (!rhi)
        return;

    auto *rhiImplementation = static_cast<QRhiImplementation *>(
        W_PRIVATE_MEMBER(*rhi, QRhi_d_tag {}));
    if (!rhiImplementation)
        return;

    auto &registeredResources = W_PRIVATE_MEMBER(*rhiImplementation, QRhiResources_tag {});
    const auto resources = registeredResources.keys();
    int releasedCount = 0;
    for (QRhiResource *resource : resources) {
        if (!resource)
            continue;
        switch (resource->resourceType()) {
        case QRhiResource::Buffer:
        case QRhiResource::Texture:
        case QRhiResource::RenderBuffer:
            ++releasedCount;
            qCWarning(lcWlRenderHelper)
                << "Force-releasing leaked QRhi graphics resource at teardown"
                << "type" << int(resource->resourceType())
                << "rhi" << Qt::hex << quintptr(rhi) << Qt::dec
                << "name" << (resource->name().isEmpty() ? QByteArrayLiteral("<unnamed>")
                                                         : resource->name());
            resource->destroy();
            break;
        default:
            break;
        }
    }
    if (releasedCount) {
        qCWarning(lcWlRenderHelper)
            << "Force-released leaked QRhi graphics resources before Vulkan allocator teardown"
            << "count" << releasedCount;
    }
}
#endif

void WRenderHelper::addVulkanTeardownSweep(QRhi *rhi, const void *key)
{
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (!rhi || !key || rhi->backend() != QRhi::Vulkan)
        return;
    rhi->addCleanupCallback(key, [ ] (QRhi *cleanupRhi) {
        forceReleaseVulkanGraphicsResources(cleanupRhi);
    });
#else
    Q_UNUSED(rhi);
    Q_UNUSED(key);
#endif
}

struct Q_DECL_HIDDEN BufferData {
    BufferData() = default;

    ~BufferData() {
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (vulkanBackdropRhi)
            vulkanBackdropRhi->removeCleanupCallback(this);
#endif
        resetWindowRenderTarget();
    }

    WPointer<wlr_buffer> buffer;
    std::unique_ptr<WListenerOwner> bufferListenerOwner;
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
    QRhi *vulkanBackdropRhi = nullptr;
#endif
    bool vulkanBackdropUnavailableLogged = false;
    bool colorPreserved = false;

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
    }

#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Register (idempotent, keyed by this BufferData) a cleanup callback on
    // the QRhi so resetWindowRenderTarget() runs from ~QRhi's runCleanup(),
    // before the backend destroys its VmaAllocator. QRhi-backed objects
    // (render buffers, texture render targets, render-pass descriptors) must
    // not outlive the RHI: at shutdown the window tears the render control
    // down while WRenderHelper still caches them, and every allocation that
    // survives until vmaDestroyAllocator turns into a fatal "Some allocations
    // were not freed before destruction of this memory block!" assertion.
    // Only the Vulkan backend registers; the OpenGL flow is unchanged.
    void registerVulkanRhiTeardownCleanup(QRhi *rhi)
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
        // Also arm the per-QRhi safety sweep once; it force-releases any
        // graphics resource that outlives the render control on this QRhi
        // (see forceReleaseVulkanGraphicsResources). Keyed by the QRhi, so
        // repeated registrations deduplicate.
        WRenderHelper::addVulkanTeardownSweep(vulkanBackdropRhi, vulkanBackdropRhi);
    }
#endif
};

class WRenderHelper::RenderTarget::Private {
public:
    std::weak_ptr<BufferData> data;
    QQuickRenderTarget selectedTarget;
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
    if (!data)
        return {};
    return d->selectedTarget.isNull() ? data->renderTarget : d->selectedTarget;
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

static std::unique_ptr<QRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor(
    QRhiTextureRenderTarget *renderTarget, const QByteArray &targetName)
{
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    std::unique_ptr<QRhiRenderPassDescriptor> descriptor(
        renderTarget->newCompatibleRenderPassDescriptor());
    if (!descriptor || !renderTarget->rhi()
        || renderTarget->rhi()->backend() != QRhi::Vulkan) {
        return descriptor;
    }

    const auto flags = renderTarget->flags();
    const bool preserveColor = flags.testFlag(
        QRhiTextureRenderTarget::PreserveColorContents);
    const bool preserveDepthStencil = flags.testFlag(
        QRhiTextureRenderTarget::PreserveDepthStencilContents);

    // Qt's offscreen Vulkan render passes leave subpassDeps empty, and Qt does
    // not include them in its render-pass compatibility or pipeline-cache
    // key. Vulkan, however, requires otherwise compatible render passes to
    // have identical dependencies. Use the same conservative dependency for
    // every Waylib-managed target with a given attachment topology, regardless
    // of its load/store flags. Besides keeping reused pipelines compatible,
    // this makes LOAD operations depend on the preceding attachment writes and
    // layout transitions instead of Qt's write-only resource transition.
    VkPipelineStageFlags attachmentStages =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkAccessFlags sourceAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkAccessFlags destinationAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    const auto &description = renderTarget->description();
    if (description.depthTexture() || description.depthStencilBuffer()) {
        attachmentStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
            | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        sourceAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        destinationAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    auto *vulkanDescriptor = static_cast<QVkRenderPassDescriptor *>(
        descriptor.get());
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = attachmentStages;
    dependency.dstStageMask = attachmentStages;
    dependency.srcAccessMask = sourceAccess;
    dependency.dstAccessMask = destinationAccess;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    const auto dependencyMatches = [&dependency] (
                                       const VkSubpassDependency &candidate) {
        return candidate.srcSubpass == dependency.srcSubpass
            && candidate.dstSubpass == dependency.dstSubpass
            && candidate.srcStageMask == dependency.srcStageMask
            && candidate.dstStageMask == dependency.dstStageMask
            && candidate.srcAccessMask == dependency.srcAccessMask
            && candidate.dstAccessMask == dependency.dstAccessMask
            && candidate.dependencyFlags == dependency.dependencyFlags;
    };
    if (vulkanDescriptor->subpassDeps.size() == 1
        && dependencyMatches(vulkanDescriptor->subpassDeps[0])) {
        return descriptor;
    }
    if (!vulkanDescriptor->subpassDeps.isEmpty()) {
        qCWarning(lcWlRenderHelper)
            << "Cannot normalize Vulkan render-pass dependencies"
            << "target" << targetName
            << "existingDependencyCount"
            << vulkanDescriptor->subpassDeps.size();
        return {};
    }
    vulkanDescriptor->subpassDeps.append(dependency);

    // The descriptor returned above already owns a native VkRenderPass.
    // Clone it after extending the private Vulkan description so the clone's
    // native render pass is created with the dependency. Destroying the
    // temporary descriptor queues its native render pass for QRhi's normal
    // deferred destruction.
    std::unique_ptr<QRhiRenderPassDescriptor> synchronizedDescriptor(
        vulkanDescriptor->newCompatibleRenderPassDescriptor());
    if (!synchronizedDescriptor) {
        qCWarning(lcWlRenderHelper)
            << "Failed to create synchronized Vulkan render pass"
            << "target" << targetName
            << "flags" << flags.toInt();
        return {};
    }

    if (WVulkanTrace::enabled()) {
        qCDebug(lcWlRenderHelper).noquote()
            << QStringLiteral("VKTRACE event=render-pass-dependency target=%1 colorLoad=%2 depthStencilLoad=%3 stages=0x%4 sourceAccess=0x%5 destinationAccess=0x%6")
                   .arg(QString::fromUtf8(targetName))
                   .arg(preserveColor)
                   .arg(preserveDepthStencil)
                   .arg(quint32(attachmentStages), 0, 16)
                   .arg(quint32(sourceAccess), 0, 16)
                   .arg(quint32(destinationAccess), 0, 16);
    }
    return synchronizedDescriptor;
#else
    Q_UNUSED(targetName);
    return std::unique_ptr<QRhiRenderPassDescriptor>(
        renderTarget->newCompatibleRenderPassDescriptor());
#endif
}

static bool recreateRhiRenderTarget(BufferData *data, QRhiTextureRenderTarget::Flags flags)
{
    auto renderTarget = static_cast<QRhiTextureRenderTarget *>(
        data->windowRenderTarget.rt.renderTarget);
    auto &rpDesc = data->windowRenderTarget.res.rpDesc;
    Q_ASSERT(renderTarget);
    renderTarget->destroy();
    renderTarget->setFlags(flags);

    std::unique_ptr<QRhiRenderPassDescriptor> newRpDesc;
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (renderTarget->rhi() && renderTarget->rhi()->backend() == QRhi::Vulkan) {
        newRpDesc = newCompatibleRenderPassDescriptor(
            renderTarget, renderTarget->name());
    } else
#endif
    {
        // Keep non-Vulkan backends on QRhi's unmodified render-pass path.
        newRpDesc.reset(renderTarget->newCompatibleRenderPassDescriptor());
    }
    if (!newRpDesc)
        return false;
    delete rpDesc;
    rpDesc = newRpDesc.release();
    renderTarget->setRenderPassDescriptor(rpDesc);
    return renderTarget->create();
}

// Copy from qquickrendertarget.cpp
static bool createRhiRenderTarget(const QRhiColorAttachment &colorAttachment,
                                  const QSize &pixelSize,
                                  int sampleCount,
                                  QRhi *rhi,
                                  QQuickWindowRenderTarget &dst,
                                  QRhiTextureRenderTarget::Flags flags = {},
                                  bool withDepthStencil = true)
{
    // The Vulkan backdrop targets are created without a depth attachment: the
    // blitter's render node never advertises DepthAwareRendering, so Qt keeps
    // the 2D depth buffer disabled for the whole renderer that contains it and
    // the attachment would only add a full-attachment depth load/store to every
    // pass split. All other callers (including the GLES2 path) keep the depth
    // attachment exactly as before.
    std::unique_ptr<QRhiRenderBuffer> depthStencil;
    if (withDepthStencil) {
        depthStencil.reset(
            rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixelSize, sampleCount));
        if (!depthStencil->create()) {
            qCWarning(lcWlRenderHelper, "Failed to build depth-stencil buffer for QQuickRenderTarget");
            return false;
        }
    }

    QRhiTextureRenderTargetDescription rtDesc(colorAttachment);
    if (depthStencil)
        rtDesc.setDepthStencilBuffer(depthStencil.get());
    std::unique_ptr<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc, flags));
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (rhi->backend() == QRhi::Vulkan) {
        rp = newCompatibleRenderPassDescriptor(
            rt.get(), QByteArrayLiteral("WaylibTextureRenderTarget"));
        if (!rp) {
            qCWarning(lcWlRenderHelper,
                      "Failed to build Vulkan render pass for QQuickRenderTarget");
            return false;
        }
    } else
#endif
    {
        // Preserve the non-Vulkan render-target path exactly as provided by
        // QRhi; in particular, do not change GLES2 render-pass behavior.
        rp.reset(rt->newCompatibleRenderPassDescriptor());
    }
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
        if (!createRhiRenderTarget(att, rtd->pixelSize, rtd->sampleCount, rhi, dst, rtFlags))
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
        resetRenderBuffer(false);
        cleanupRetiredRenderBuffers(true);
    }

    struct RetiredBuffers {
        QList<std::shared_ptr<BufferData>> buffers;
    };

    bool shouldDeferRenderBufferCleanup() const;
    void resetRenderBuffer(bool defer);
    void cleanupRetiredRenderBuffers(bool force);
    void onBufferDestroy(BufferData *data);
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data,
                                      QRhiTextureRenderTarget::Flags flags,
                                      QRhiTexture::Flags extraTextureFlags = {});
    bool ensureVulkanBackdropRenderTargets(QQuickRenderControl *rc,
                                           BufferData *data);

    W_DECLARE_PUBLIC(WRenderHelper)
    wlr_renderer *renderer;
    QList<std::shared_ptr<BufferData>> buffers;
    QList<RetiredBuffers> retiredBuffers;
    std::weak_ptr<BufferData> lastBuffer;

    QSize size;
};

bool WRenderHelperPrivate::shouldDeferRenderBufferCleanup() const
{
#ifdef ENABLE_VULKAN_RENDER
    return renderer && wlr_renderer_is_vk(renderer);
#else
    return false;
#endif
}

void WRenderHelperPrivate::resetRenderBuffer(bool defer)
{
    if (defer && !buffers.isEmpty()) {
        retiredBuffers.append({buffers});
    }
    buffers.clear();
    lastBuffer.reset();
}

void WRenderHelperPrivate::cleanupRetiredRenderBuffers(bool force)
{
    Q_UNUSED(force);
    retiredBuffers.clear();
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

bool WRenderHelperPrivate::ensureRhiRenderTarget(QQuickRenderControl *rc,
                                                 BufferData *data,
                                                 QRhiTextureRenderTarget::Flags flags,
                                                 QRhiTexture::Flags extraTextureFlags)
{
    data->resetWindowRenderTarget();
    auto rhi = rc->rhi();
    auto tmp = data->renderTarget;
    bool ok = createRhiRenderTarget(rhi, tmp, data->windowRenderTarget,
                                    flags, extraTextureFlags);
    if (!ok)
        return false;
    data->renderTarget = QQuickRenderTarget::fromRhiRenderTarget(data->windowRenderTarget.rt.renderTarget);
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

#if defined(ENABLE_VULKAN_RENDER) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Every BufferData owning QRhi objects must release them from ~QRhi's
    // runCleanup(), before the VmaAllocator they are backed by dies (see
    // registerVulkanRhiTeardownCleanup).
    if (rhi->backend() == QRhi::Vulkan)
        data->registerVulkanRhiTeardownCleanup(rhi);
#endif
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
    // The blitter's Vulkan render node never advertises DepthAwareRendering, so
    // Qt disables the 2D depth buffer for the whole batch renderer as soon as a
    // blitter node exists. The backdrop targets are therefore built without a
    // depth attachment (see createRhiRenderTarget): the depth contents are never
    // read, and keeping it would add a full-attachment depth load/store plus a
    // depth barrier to every pass split. WRenderBufferNode::prepare() fails
    // closed if a depth-using renderer ever reaches the blitter.
    const bool sameDevice = renderer && wlr_renderer_is_vk(renderer) && nativeHandles
        && nativeHandles->physDev == wlr_vk_renderer_get_physical_device(renderer)
        && nativeHandles->dev == wlr_vk_renderer_get_device(renderer)
        && nativeHandles->gfxQueueFamilyIdx == wlr_vk_renderer_get_queue_family(renderer)
        && nativeHandles->gfxQueue == waylib_vk_renderer_get_queue(renderer);
    if (!rhi || rhi->backend() != QRhi::Vulkan || !colorTexture
        || colorTexture->sampleCount() != 1
        || !colorTexture->flags().testFlag(QRhiTexture::UsedAsTransferSource)
        || !sameDevice) {
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

    const QSize pixelSize = colorTexture->pixelSize();
    const int sampleCount = colorTexture->sampleCount();
    const QRhiColorAttachment colorAttachment(colorTexture);

    bool ok = createRhiRenderTarget(colorAttachment, pixelSize, sampleCount, rhi,
                                    data->vulkanBackdropWindowRenderTarget,
                                    QRhiTextureRenderTarget::Flags{},
                                    /*withDepthStencil=*/false);
    if (ok) {
        ok = createRhiRenderTarget(colorAttachment, pixelSize, sampleCount, rhi,
                                   data->vulkanBackdropPreserveWindowRenderTarget,
                                   QRhiTextureRenderTarget::PreserveColorContents,
                                   /*withDepthStencil=*/false);
    }
    if (ok) {
        // Only color needs to be preserved now: with no depth attachment the
        // PreserveDepthStencilContents flag would be a no-op, and dropping it
        // keeps this render pass compatible with the preserve target's.
        ok = createRhiRenderTarget(colorAttachment, pixelSize, sampleCount, rhi,
                                   data->vulkanBackdropResumeWindowRenderTarget,
                                   QRhiTextureRenderTarget::PreserveColorContents,
                                   /*withDepthStencil=*/false);
    }

    if (!ok) {
        BufferData::cleanupWindowRenderTarget(data->vulkanBackdropResumeWindowRenderTarget);
        BufferData::cleanupWindowRenderTarget(data->vulkanBackdropPreserveWindowRenderTarget);
        BufferData::cleanupWindowRenderTarget(data->vulkanBackdropWindowRenderTarget);
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

    // Idempotent: usually already registered by ensureRhiRenderTarget().
    data->registerVulkanRhiTeardownCleanup(rhi);

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
            << QStringLiteral("VKTRACE event=blitter-targets-create buffer=%1 color=%2 clear=%3 preserveColor=%4 resume=%5 size=%6x%7")
                   .arg(quintptr(data->buffer.get()), 0, 16)
                   .arg(quintptr(colorTexture), 0, 16)
                   .arg(quintptr(data->vulkanBackdropWindowRenderTarget.rt.renderTarget), 0, 16)
                   .arg(quintptr(data->vulkanBackdropPreserveWindowRenderTarget.rt.renderTarget), 0, 16)
                   .arg(quintptr(data->vulkanBackdropResumeWindowRenderTarget.rt.renderTarget), 0, 16)
                   .arg(pixelSize.width())
                   .arg(pixelSize.height());
    }

    return true;
#endif
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
    d->resetRenderBuffer(d->shouldDeferRenderBufferCleanup());

    Q_EMIT sizeChanged();
}

void WRenderHelper::cleanupRetiredRenderResources(bool force)
{
    W_D(WRenderHelper);
    d->cleanupRetiredRenderBuffers(force);
}

bool WRenderHelper::acquireRenderBuffer(QQuickRenderControl *rc, wlr_buffer *buffer, const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    W_D(WRenderHelper);
    if (!d->renderer || !wlr_renderer_is_vk(d->renderer))
        return true;

    wlr_vk_image_attribs attribs = {};
    if (!getVulkanRenderBufferAttribs(d->renderer, buffer, purpose, &attribs))
        return false;

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed: missing Vulkan QRhi"
                                    << "purpose" << purpose
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    // Queue-family ownership barriers are illegal inside a pass instance; all
    // render buffer acquires must be recorded between QRhi passes.
    if (Q_UNLIKELY(qtVulkanPassIsRecording(commandBuffer))) {
        commandBuffer->endExternal();
        qCCritical(lcWlRenderHelper)
            << "Vulkan render buffer acquire requested inside an active QRhi pass"
            << "purpose" << purpose << "buffer" << buffer;
        return false;
    }

    const bool ok = waylib_vk_renderer_record_render_buffer_acquire(
        d->renderer, buffer, handles->commandBuffer);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer acquire failed"
                                    << "purpose" << purpose
                                    << "buffer" << buffer
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    qCDebug(lcWlRenderHelper) << "Vulkan render buffer acquire recorded"
                              << "purpose" << purpose
                              << "buffer" << buffer
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
                                        wlr_buffer *buffer,
                                        QRhiTexture *renderTargetTexture,
                                        const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    W_D(WRenderHelper);
    if (!d->renderer || !wlr_renderer_is_vk(d->renderer))
        return true;

    wlr_vk_image_attribs attribs = {};
    if (!getVulkanRenderBufferAttribs(d->renderer, buffer, purpose, &attribs))
        return false;

    if (!renderTargetTexture) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing Qt render target texture"
                                    << "purpose" << purpose
                                    << "buffer" << buffer
                                    << "image" << vkImageName(attribs.image)
                                    << "layout" << vkImageLayoutName(attribs.layout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing Vulkan QRhi"
                                    << "purpose" << purpose
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
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
                                    << "buffer" << buffer
                                    << "image" << vkImageName(attribs.image)
                                    << "wlrootsLayout" << vkImageLayoutName(attribs.layout)
                                    << "oldLayout" << vkImageLayoutName(oldLayout)
                                    << "format" << hex32(attribs.format)
                                    << "size" << wlrBufferSize(buffer);
        return false;
    }

    // Ownership-release barriers are illegal inside a pass instance.
    if (Q_UNLIKELY(qtVulkanPassIsRecording(commandBuffer))) {
        commandBuffer->endExternal();
        qCCritical(lcWlRenderHelper)
            << "Vulkan render buffer release requested inside an active QRhi pass"
            << "purpose" << purpose << "buffer" << buffer;
        return false;
    }

    const bool ok = waylib_vk_renderer_record_render_buffer_release(
        d->renderer, buffer, handles->commandBuffer, oldLayout);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed"
                                    << "purpose" << purpose
                                    << "buffer" << buffer
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
                              << "buffer" << buffer
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
                                                               WGlobal::ColorContentsMode mode,
                                                               bool useVulkanBackdrop)
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
            if (d->shouldDeferRenderBufferCleanup()
                && !acquireRenderBuffer(rc, buffer,
                                        "cached-compositor-render-target")) {
                return {};
            }
            RenderTarget result;
            result.d->data = data;
            if (useVulkanBackdrop
                && d->ensureVulkanBackdropRenderTargets(rc, data.get())) {
                result.d->selectedTarget = data->vulkanBackdropRenderTarget;
            }
            return result;
        }
    }

    std::unique_ptr<BufferData> bufferData(new BufferData);
    bufferData->buffer = buffer;
    bufferData->colorPreserved = needPreserve;

    QQuickRenderTarget rt;
    bool needsVulkanRenderBufferAcquire = false;
    QRhiTexture::Flags renderTargetTextureFlags;

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
                                      << "buffer" << buffer
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "usage" << hex32(attribs.usage)
                                      << "size" << wlrBufferSize(buffer);
        }
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
            if (!d->ensureRhiRenderTarget(rc, bufferData.get(), flags,
                                          renderTargetTextureFlags))
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
                                         texture, bufferData->buffer.get() });
            if (bufferData->preserveWindowRenderTarget.rt.renderTarget) {
                s_rhiRenderBuffers->append({ bufferData->preserveWindowRenderTarget.rt.renderTarget,
                                             texture, bufferData->buffer.get() });
            }
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
    if (useVulkanBackdrop
        && d->ensureVulkanBackdropRenderTargets(rc, d->buffers.last().get())) {
        result.d->selectedTarget = d->buffers.last()->vulkanBackdropRenderTarget;
    }
    return result;
}

QQuickRenderTarget WRenderHelper::preserveRenderTarget(wlr_buffer *buffer,
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

QQuickRenderTarget WRenderHelper::vulkanBackdropResumeRenderTarget(wlr_buffer *buffer) const
{
    W_DC(WRenderHelper);
    for (auto data : std::as_const(d->buffers)) {
        if (data->buffer == buffer)
            return data->vulkanBackdropResumeRenderTarget;
    }

    return {};
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
            WVulkanTrace::activate(getGraphicsApi() == QSGRendererInterface::Vulkan);
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

    WVulkanTrace::activate(getGraphicsApi() == QSGRendererInterface::Vulkan);
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

static bool updateGLTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture, bool) {
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
    return true;
}

static inline quint64 vkimage_cast(void *image) {
    return reinterpret_cast<quintptr>(image);
}

[[maybe_unused]] static inline quint64 vkimage_cast(quint64 image) {
    return image;
}

#ifdef ENABLE_VULKAN_RENDER
static bool updateVKTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture,
                            bool forceShaderReadOnlyLayout)
{
    if (!rhi || rhi->backend() != QRhi::Vulkan)
        return false;

    wlr_vk_image_attribs attribs = {};
    wlr_vk_texture_get_image_attribs(handle, &attribs);
    const QSize size(handle->width, handle->height);
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
                                    << "wlrTexture" << handle
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
                                    << "wlrTexture" << handle
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
                                    << "wlrTexture" << handle
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
                                    << "wlrTexture" << handle
                                    << "image" << vkImageName(attribs.image)
                                    << "format" << hex32(attribs.format)
                                    << "hasInitialView" << bool(vkTexture->imageView != VK_NULL_HANDLE)
                                    << "lastActiveFrameSlot" << vkTexture->lastActiveFrameSlot;
        discardUnusedCandidate();
        return false;
    }

    const bool hasAlpha = wlr_vk_texture_has_alpha(handle);
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
                                    << "wlrTexture" << handle
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
                                << "wlrTexture" << handle
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

static bool updateImage(QRhi *, wlr_texture *handle, QSGPlainTexture *texture, bool) {
    auto image = wlr_pixman_texture_get_image(handle);
    texture->setImage(WTools::fromPixmanImage(image));
    return true;
}

typedef bool(*UpdateTextureFunction)(QRhi *, wlr_texture *, QSGPlainTexture *, bool);

static UpdateTextureFunction getUpdateTextFunction(wlr_texture *handle)
{
    const auto api = WRenderHelper::getGraphicsApi();
    if (api == QSGRendererInterface::OpenGL) {
        if (!wlr_texture_is_gles2(handle))
            return nullptr;
        return updateGLTexture;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (api == QSGRendererInterface::Vulkan) {
        if (!wlr_texture_is_vk(handle))
            return nullptr;
        return updateVKTexture;
    }
#endif
    else if (api == QSGRendererInterface::Software) {
        if (!wlr_texture_is_pixman(handle))
            return nullptr;
        return updateImage;
    }

    return nullptr;
}

bool WRenderHelper::makeTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture,
                                bool forceVulkanShaderReadOnlyLayout)
{
    auto updateTexture = getUpdateTextFunction(handle);
    if (Q_UNLIKELY(!updateTexture))
        return false;
    return updateTexture(rhi, handle, texture, forceVulkanShaderReadOnlyLayout);
}

bool WRenderHelper::beginTextureSyncBatch(QQuickRenderControl *rc,
                                          wlr_renderer *renderer,
                                          bool verifyQueue)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !wlr_renderer_is_vk(renderer))
        return true;

    if (verifyQueue) {
        if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
            qCWarning(lcWlRenderHelper) << "Cannot begin Vulkan texture sync batch: missing Vulkan QRhi";
            return false;
        }

        const auto *nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(rc->rhi()->nativeHandles());
        if (!nativeHandles
            || nativeHandles->dev != wlr_vk_renderer_get_device(renderer)
            || nativeHandles->gfxQueueFamilyIdx != wlr_vk_renderer_get_queue_family(renderer)
            || nativeHandles->gfxQueue != waylib_vk_renderer_get_queue(renderer)) {
            qCWarning(lcWlRenderHelper) << "Cannot begin Vulkan texture sync batch: Qt and wlroots do not share the same queue"
                                        << "qtDevice" << (nativeHandles ? nativeHandles->dev : VK_NULL_HANDLE)
                                        << "wlrootsDevice" << wlr_vk_renderer_get_device(renderer)
                                        << "qtQueueFamily" << (nativeHandles ? nativeHandles->gfxQueueFamilyIdx : 0)
                                        << "wlrootsQueueFamily" << wlr_vk_renderer_get_queue_family(renderer);
            return false;
        }
    }

    if (!waylib_vk_renderer_begin_texture_sync_batch(renderer)) {
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

bool WRenderHelper::flushTextureSyncBatch(wlr_renderer *renderer)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !wlr_renderer_is_vk(renderer))
        return true;

    if (!waylib_vk_renderer_flush_texture_sync_batch(renderer)) {
        qCWarning(lcWlRenderHelper) << "Failed to flush Vulkan texture sync batch";
        return false;
    }
#else
    Q_UNUSED(renderer);
#endif
    return true;
}

void WRenderHelper::abortTextureSyncBatch(wlr_renderer *renderer)
{
#ifdef ENABLE_VULKAN_RENDER
    if (renderer && wlr_renderer_is_vk(renderer))
        waylib_vk_renderer_abort_texture_sync_batch(renderer);
#else
    Q_UNUSED(renderer);
#endif
}

bool WRenderHelper::beginTextureBarrierBatch(wlr_renderer *renderer, bool release)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !wlr_renderer_is_vk(renderer))
        return true;

    if (!waylib_vk_renderer_begin_texture_barrier_batch(renderer, release)) {
        qCWarning(lcWlRenderHelper) << "Failed to begin Vulkan texture barrier batch"
                                    << "release" << release;
        return false;
    }
#else
    Q_UNUSED(renderer);
    Q_UNUSED(release);
#endif
    return true;
}

bool WRenderHelper::flushTextureBarrierBatch(QQuickRenderControl *rc,
                                             wlr_renderer *renderer,
                                             const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !wlr_renderer_is_vk(renderer))
        return true;

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlRenderHelper) << "Cannot flush Vulkan texture barrier batch: missing Vulkan QRhi"
                                    << "purpose" << purpose;
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
        return false;
    }

    if (rc->rhi()->isDeviceLost() || !rc->rhi()->isRecordingFrame()) {
        qCWarning(lcWlRenderHelper) << "Cannot flush Vulkan texture barrier batch: QRhi frame is not usable"
                                    << "purpose" << purpose
                                    << "deviceLost" << rc->rhi()->isDeviceLost()
                                    << "recordingFrame" << rc->rhi()->isRecordingFrame();
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
        return false;
    }

    auto commandBuffer = rc->commandBuffer();
    if (!commandBuffer) {
        qCWarning(lcWlRenderHelper) << "Cannot flush Vulkan texture barrier batch: missing QRhi command buffer"
                                    << "purpose" << purpose;
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
        return false;
    }

    commandBuffer->beginExternal();
    auto handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(commandBuffer->nativeHandles());
    if (!handles || handles->commandBuffer == VK_NULL_HANDLE) {
        commandBuffer->endExternal();
        qCWarning(lcWlRenderHelper) << "Cannot flush Vulkan texture barrier batch: missing native Vulkan command buffer"
                                    << "purpose" << purpose;
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
        return false;
    }

    // Ownership/layout barrier batches must be recorded between QRhi passes.
    if (Q_UNLIKELY(qtVulkanPassIsRecording(commandBuffer))) {
        commandBuffer->endExternal();
        qCCritical(lcWlRenderHelper)
            << "Vulkan texture barrier batch flush requested inside an active QRhi pass"
            << "purpose" << purpose;
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
        return false;
    }

    const bool ok = waylib_vk_renderer_flush_texture_barrier_batch(
        renderer, handles->commandBuffer);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlRenderHelper) << "Failed to flush Vulkan texture barrier batch"
                                    << "purpose" << purpose;
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
    }
    return ok;
#else
    Q_UNUSED(rc);
    Q_UNUSED(renderer);
    Q_UNUSED(purpose);
    return true;
#endif
}

void WRenderHelper::abortTextureBarrierBatch(wlr_renderer *renderer)
{
#ifdef ENABLE_VULKAN_RENDER
    if (renderer && wlr_renderer_is_vk(renderer))
        waylib_vk_renderer_abort_texture_barrier_batch(renderer);
#else
    Q_UNUSED(renderer);
#endif
}

void WRenderHelper::setStageAsyncEnabled(wlr_renderer *renderer, bool enabled)
{
#ifdef ENABLE_VULKAN_RENDER
    if (renderer && wlr_renderer_is_vk(renderer))
        waylib_vk_renderer_set_stage_async_enabled(renderer, enabled);
#else
    Q_UNUSED(renderer);
    Q_UNUSED(enabled);
#endif
}

bool WRenderHelper::prepareTextureForSampling(QQuickRenderControl *rc,
                                              wlr_renderer *renderer,
                                              wlr_texture *texture,
                                              const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !texture)
        return true;

    if (!wlr_renderer_is_vk(renderer))
        return true;

    if (!wlr_texture_is_vk(texture)) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: non-Vulkan wlroots texture"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture;
        return false;
    }

    wlr_vk_image_attribs rawAttribs = {};
    wlr_vk_texture_get_image_attribs(texture, &rawAttribs);

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed: missing Vulkan QRhi"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture
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
                                      << "wlrTexture" << texture
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
                                      << "wlrTexture" << texture
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
                                      << "wlrTexture" << texture
                                      << "image" << vkImageName(rawAttribs.image)
                                      << "wlrootsLayout" << vkImageLayoutName(rawAttribs.layout)
                                      << "format" << hex32(rawAttribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    // Acquire ownership barriers inside a pass instance are illegal; texture
    // acquisition must happen between QRhi passes.
    if (Q_UNLIKELY(qtVulkanPassIsRecording(commandBuffer))) {
        commandBuffer->endExternal();
        qCCritical(lcWlQtQuickTexture)
            << "Vulkan texture sampling prepare requested inside an active QRhi pass"
            << "purpose" << purpose << "wlrTexture" << texture;
        return false;
    }

    const bool ok = waylib_vk_renderer_prepare_texture_for_sampling(
        renderer, texture, handles->commandBuffer, nullptr);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling prepare failed"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture
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
                                          wlr_renderer *renderer,
                                          wlr_texture *texture,
                                          const char *purpose)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!renderer || !texture)
        return true;

    if (!wlr_renderer_is_vk(renderer))
        return true;

    if (!wlr_texture_is_vk(texture)) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: non-Vulkan wlroots texture"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture;
        return false;
    }

    wlr_vk_image_attribs attribs = {};
    wlr_vk_texture_get_image_attribs(texture, &attribs);

    if (!rc || !rc->rhi() || rc->rhi()->backend() != QRhi::Vulkan) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed: missing Vulkan QRhi"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture
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
                                      << "wlrTexture" << texture
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
                                      << "wlrTexture" << texture
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
                                      << "wlrTexture" << texture
                                      << "image" << vkImageName(attribs.image)
                                      << "layout" << vkImageLayoutName(attribs.layout)
                                      << "format" << hex32(attribs.format)
                                      << "size" << wlrTextureSize(texture);
        return false;
    }

    // Release-ownership barriers are illegal inside a pass instance.
    if (Q_UNLIKELY(qtVulkanPassIsRecording(commandBuffer))) {
        commandBuffer->endExternal();
        qCCritical(lcWlQtQuickTexture)
            << "Vulkan texture sampling finish requested inside an active QRhi pass"
            << "purpose" << purpose << "wlrTexture" << texture;
        return false;
    }

    const bool ok = waylib_vk_renderer_finish_texture_sampling(
        renderer, texture, handles->commandBuffer);
    commandBuffer->endExternal();

    if (!ok) {
        qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed"
                                      << "purpose" << purpose
                                      << "wlrTexture" << texture
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

        wlr_vk_image_attribs attribs = {};
        if (!waylib_vk_renderer_get_render_buffer_attribs(renderer, buffer, &attribs)) {
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
