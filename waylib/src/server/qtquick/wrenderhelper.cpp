// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wrenderhelper.h"
#include "wtools.h"
#include "wayliblogging.h"
#include "private/wqmlhelper_p.h"
#include "private/wglobal_p.h"

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
#include <qwcompositor.h>
#include <qwlinuxdrmsyncobjv1.h>
#include <qwrendererinterface.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QByteArray>
#include <QVulkanInstance>
#include <QVarLengthArray>
#include <QVector>

#include <QSGTexture>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qsgplaintexture_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qsgsoftwarepixmaptexture_p.h>
#include <private/qsgrhisupport_p.h>

extern "C" {
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/dmabuf.h>
#include <linux/dma-buf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <errno.h>
#endif
}
#include <drm_fourcc.h>
#include <dlfcn.h>
#include <xf86drm.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN RhiRenderEntry {
    const QRhiRenderTarget *renderTarget;
    const QRhiTexture *texture;
    QPointer<qw_buffer> buffer;
};

Q_GLOBAL_STATIC(QVector<RhiRenderEntry>, s_rhiRenderBuffers)

static void removeRhiRenderBufferEntry(const QRhiRenderTarget *renderTarget)
{
    if (!renderTarget)
        return;

    auto it = s_rhiRenderBuffers->begin();
    while (it != s_rhiRenderBuffers->end()) {
        if (it->renderTarget == renderTarget) {
            it = s_rhiRenderBuffers->erase(it);
        } else {
            ++it;
        }
    }
}

static const char *quickRenderTargetTypeName(QQuickRenderTargetPrivate::Type type)
{
    switch (type) {
    case QQuickRenderTargetPrivate::Type::Null:
        return "Null";
    case QQuickRenderTargetPrivate::Type::NativeTexture:
        return "NativeTexture";
    case QQuickRenderTargetPrivate::Type::NativeTextureArray:
        return "NativeTextureArray";
    case QQuickRenderTargetPrivate::Type::NativeRenderbuffer:
        return "NativeRenderbuffer";
    case QQuickRenderTargetPrivate::Type::RhiRenderTarget:
        return "RhiRenderTarget";
    case QQuickRenderTargetPrivate::Type::PaintDevice:
        return "PaintDevice";
    }

    return "Unknown";
}

#ifdef ENABLE_VULKAN_RENDER
struct Q_DECL_HIDDEN VulkanImportedRenderTarget {
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memories[WLR_DMABUF_MAX_PLANES] = {};
    uint32_t memoryCount = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool ownerIsForeign = true;
    bool scanoutReleaseReady = false;
    QSize size;
    uint32_t drmFormat = 0;
    uint64_t drmModifier = DRM_FORMAT_MOD_INVALID;

    bool isValid() const {
        return device != VK_NULL_HANDLE && image != VK_NULL_HANDLE
            && format != VK_FORMAT_UNDEFINED && !size.isEmpty();
    }
};

struct Q_DECL_HIDDEN VulkanImportedNativeTexture {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memories[WLR_DMABUF_MAX_PLANES] = {};
    uint32_t memoryCount = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    QSize size;
    uint32_t drmFormat = 0;
    uint64_t drmModifier = DRM_FORMAT_MOD_INVALID;

    bool isValid() const {
        return device != VK_NULL_HANDLE && image != VK_NULL_HANDLE
            && format != VK_FORMAT_UNDEFINED && !size.isEmpty();
    }
};
#endif

struct Q_DECL_HIDDEN BufferData {
    BufferData() {

    }

    ~BufferData() {
        resetWindowRenderTarget();
#ifdef ENABLE_VULKAN_RENDER
        destroyEglDmabufTexture();
        destroyVulkanRenderTarget();
#endif
    }

    qw_buffer *buffer = nullptr;
#ifdef ENABLE_VULKAN_RENDER
    // EGL dmabuf import state (used when wlroots renderer is Vulkan but Qt RHI
    // is GL). The dmabuf is imported as an EGLImage, then bound to a GL texture
    // via glEGLImageTargetTexture2DOES. This bypasses wlroots's texture system
    // entirely — dmabuf is API-agnostic, any EGL context can import it.
    EGLImage eglImage = EGL_NO_IMAGE;
    GLuint glTexture = 0;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;

    void destroyEglDmabufTexture();
    void destroyVulkanRenderTarget();

    VulkanImportedRenderTarget vulkanRenderTarget;
#endif
    // for software renderer
    WImageRenderTarget paintDevice;
    QQuickRenderTarget renderTarget;
    QQuickRenderTarget preserveRenderTarget;
    QQuickWindowRenderTarget windowRenderTarget;
    QQuickWindowRenderTarget preserveWindowRenderTarget;

    inline void resetWindowRenderTarget() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        removeRhiRenderBufferEntry(windowRenderTarget.rt.renderTarget);
        removeRhiRenderBufferEntry(preserveWindowRenderTarget.rt.renderTarget);

        if (preserveWindowRenderTarget.rt.owns)
            delete preserveWindowRenderTarget.rt.renderTarget;

        delete preserveWindowRenderTarget.res.renderBuffer;
        delete preserveWindowRenderTarget.res.rpDesc;

        preserveWindowRenderTarget.rt = {};
        preserveWindowRenderTarget.res = {};
        {
            delete preserveWindowRenderTarget.implicitBuffers.depthStencil;
            delete preserveWindowRenderTarget.implicitBuffers.depthStencilTexture;
            delete preserveWindowRenderTarget.implicitBuffers.multisampleTexture;
            preserveWindowRenderTarget.implicitBuffers = {};
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
        preserveWindowRenderTarget.sw = {};
#else
        removeRhiRenderBufferEntry(windowRenderTarget.renderTarget);
        removeRhiRenderBufferEntry(preserveWindowRenderTarget.renderTarget);

        if (preserveWindowRenderTarget.owns) {
            delete preserveWindowRenderTarget.renderTarget;
            delete preserveWindowRenderTarget.rpDesc;
            delete preserveWindowRenderTarget.renderBuffer;
            delete preserveWindowRenderTarget.depthStencil;
            delete preserveWindowRenderTarget.paintDevice;
        }

        preserveWindowRenderTarget.renderTarget = nullptr;
        preserveWindowRenderTarget.rpDesc = nullptr;
        preserveWindowRenderTarget.texture = nullptr;
        preserveWindowRenderTarget.renderBuffer = nullptr;
        preserveWindowRenderTarget.depthStencil = nullptr;
        preserveWindowRenderTarget.paintDevice = nullptr;
        preserveWindowRenderTarget.owns = false;

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
        preserveRenderTarget = {};
    }
};

#ifdef ENABLE_VULKAN_RENDER
// Resolve Vulkan loader entry points via dlsym. wlroots links libvulkan, but
// waylib does not link it directly in all builds, so keep Vulkan calls behind
// runtime-resolved function pointers.
static PFN_vkGetDeviceProcAddr resolveVkGetDeviceProcAddr()
{
    static PFN_vkGetDeviceProcAddr proc =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(dlsym(RTLD_DEFAULT, "vkGetDeviceProcAddr"));
    return proc;
}

static PFN_vkGetInstanceProcAddr resolveVkGetInstanceProcAddr()
{
    static PFN_vkGetInstanceProcAddr proc =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr"));
    return proc;
}

static QByteArray drmFormatToName(uint32_t format)
{
    char *name = drmGetFormatName(format);
    if (!name)
        return QByteArray::number(format, 16);

    QByteArray result(name);
    free(name);
    return result;
}

static QByteArray drmModifierToName(uint64_t modifier)
{
    if (modifier == DRM_FORMAT_MOD_INVALID)
        return QByteArrayLiteral("INVALID");

    char *name = drmGetFormatModifierName(modifier);
    if (!name)
        return QByteArray::number(modifier, 16);

    QByteArray result(name);
    free(name);
    return result;
}

static const char *vkResultName(VkResult result)
{
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
#ifdef VK_ERROR_UNKNOWN
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
#endif
    default: return "VK_RESULT_UNKNOWN";
    }
}

static const char *vkImageLayoutName(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
    case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL";
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL";
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED";
#ifdef VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
#endif
    default: return "OTHER";
    }
}

template<typename Handle>
static quint64 vulkanHandleToInteger(Handle handle)
{
    if constexpr (std::is_pointer_v<Handle>)
        return quint64(reinterpret_cast<quintptr>(handle));
    else
        return quint64(handle);
}

struct Q_DECL_HIDDEN VulkanInteropCommandFunctions {
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;
    PFN_vkGetFenceStatus vkGetFenceStatus = nullptr;
    PFN_vkWaitForFences vkWaitForFences = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR = nullptr;

    bool resolve(VkDevice device, bool needSyncFileExport,
                 const char *subject, const char *phase)
    {
        auto vkGetDeviceProcAddr = resolveVkGetDeviceProcAddr();
        if (!vkGetDeviceProcAddr) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "failed: vkGetDeviceProcAddr unavailable";
            return false;
        }

        vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(
            vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
        vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(
            vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
        vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
            vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
        vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
            vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
        vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
            vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
        vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(
            vkGetDeviceProcAddr(device, "vkResetCommandBuffer"));
        vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
            vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier"));
        vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
            vkGetDeviceProcAddr(device, "vkQueueSubmit"));
        vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(
            vkGetDeviceProcAddr(device, "vkCreateFence"));
        vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(
            vkGetDeviceProcAddr(device, "vkDestroyFence"));
        vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(
            vkGetDeviceProcAddr(device, "vkGetFenceStatus"));
        vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(
            vkGetDeviceProcAddr(device, "vkWaitForFences"));
        vkResetFences = reinterpret_cast<PFN_vkResetFences>(
            vkGetDeviceProcAddr(device, "vkResetFences"));
        vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
            vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
        vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
            vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
        vkGetSemaphoreFdKHR = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
            vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));

        if (!vkCreateCommandPool || !vkDestroyCommandPool || !vkAllocateCommandBuffers
            || !vkBeginCommandBuffer || !vkEndCommandBuffer || !vkResetCommandBuffer
            || !vkCmdPipelineBarrier || !vkQueueSubmit || !vkCreateFence || !vkDestroyFence
            || !vkGetFenceStatus || !vkWaitForFences || !vkResetFences
            || (needSyncFileExport && (!vkCreateSemaphore || !vkDestroySemaphore
                                       || !vkGetSemaphoreFdKHR))) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "failed: required Vulkan command/sync functions unavailable"
                << "exportSyncFile" << needSyncFileExport;
            return false;
        }

        return true;
    }
};

struct Q_DECL_HIDDEN VulkanInteropCommandSlot {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    bool busy = false;
};

struct Q_DECL_HIDDEN VulkanInteropCommandContext {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VulkanInteropCommandFunctions api;
    QVector<VulkanInteropCommandSlot> slots;

    bool matches(VkDevice dev, VkQueue q, uint32_t family) const
    {
        return device == dev && queue == q && queueFamilyIndex == family;
    }

    bool init(VkDevice dev, VkQueue q, uint32_t family,
              bool needSyncFileExport, const char *subject, const char *phase)
    {
        device = dev;
        queue = q;
        queueFamilyIndex = family;
        if (!api.resolve(device, needSyncFileExport, subject, phase))
            return false;

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        VkResult res = api.vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "failed: vkCreateCommandPool"
                << vkResultName(res) << int(res);
            commandPool = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool ensureSyncFileExportFunctions(const char *subject, const char *phase)
    {
        if (api.vkCreateSemaphore && api.vkDestroySemaphore && api.vkGetSemaphoreFdKHR)
            return true;
        return api.resolve(device, true, subject, phase);
    }

    bool slotReady(VulkanInteropCommandSlot *slot, bool wait,
                   const char *subject, const char *phase)
    {
        if (!slot || !slot->busy || slot->fence == VK_NULL_HANDLE)
            return true;

        VkResult res = VK_SUCCESS;
        if (wait) {
            res = api.vkWaitForFences(device, 1, &slot->fence, VK_TRUE, UINT64_MAX);
        } else {
            res = api.vkGetFenceStatus(device, slot->fence);
        }

        if (res == VK_NOT_READY)
            return false;
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "command fence status failed"
                << vkResultName(res) << int(res)
                << "fence" << Qt::hex << vulkanHandleToInteger(slot->fence) << Qt::dec;
        }

        slot->busy = false;
        return true;
    }

    void retireCompletedSlots(bool wait)
    {
        for (auto &slot : slots)
            slotReady(&slot, wait, "Vulkan RHI interop", "retire");
    }

    void destroy(bool wait)
    {
        retireCompletedSlots(wait);

        for (auto &slot : slots) {
            if (slot.semaphore != VK_NULL_HANDLE && api.vkDestroySemaphore)
                api.vkDestroySemaphore(device, slot.semaphore, nullptr);
            if (slot.fence != VK_NULL_HANDLE && api.vkDestroyFence)
                api.vkDestroyFence(device, slot.fence, nullptr);
        }
        slots.clear();

        if (commandPool != VK_NULL_HANDLE && api.vkDestroyCommandPool)
            api.vkDestroyCommandPool(device, commandPool, nullptr);

        commandPool = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        queue = VK_NULL_HANDLE;
        queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    VulkanInteropCommandSlot *acquireSlot(bool exportSyncFile,
                                          const char *subject, const char *phase)
    {
        if (exportSyncFile && !ensureSyncFileExportFunctions(subject, phase))
            return nullptr;

        retireCompletedSlots(false);

        for (auto &slot : slots) {
            if (!slot.busy)
                return ensureSlotObjects(&slot, exportSyncFile, subject, phase) ? &slot : nullptr;
        }

        static constexpr qsizetype maxCachedSlots = 8;
        if (slots.size() < maxCachedSlots) {
            slots.append(VulkanInteropCommandSlot{});
            auto &slot = slots.last();
            return ensureSlotObjects(&slot, exportSyncFile, subject, phase) ? &slot : nullptr;
        }

        VulkanInteropCommandSlot *slot = &slots.first();
        if (!slotReady(slot, true, subject, phase))
            return nullptr;
        return ensureSlotObjects(slot, exportSyncFile, subject, phase) ? slot : nullptr;
    }

    bool ensureSlotObjects(VulkanInteropCommandSlot *slot, bool exportSyncFile,
                           const char *subject, const char *phase)
    {
        if (!slot)
            return false;

        VkResult res = VK_SUCCESS;
        if (slot->commandBuffer == VK_NULL_HANDLE) {
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            res = api.vkAllocateCommandBuffers(device, &allocInfo, &slot->commandBuffer);
            if (res != VK_SUCCESS) {
                qCWarning(lcWlRenderHelper)
                    << subject << phase
                    << "failed: vkAllocateCommandBuffers"
                    << vkResultName(res) << int(res);
                return false;
            }
        }

        if (slot->fence == VK_NULL_HANDLE) {
            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

            res = api.vkCreateFence(device, &fenceInfo, nullptr, &slot->fence);
            if (res != VK_SUCCESS) {
                qCWarning(lcWlRenderHelper)
                    << subject << phase
                    << "failed: vkCreateFence"
                    << vkResultName(res) << int(res);
                return false;
            }
        }

        res = api.vkResetFences(device, 1, &slot->fence);
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "failed: vkResetFences"
                << vkResultName(res) << int(res);
            return false;
        }

        res = api.vkResetCommandBuffer(slot->commandBuffer, 0);
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "failed: vkResetCommandBuffer"
                << vkResultName(res) << int(res);
            return false;
        }

        if (exportSyncFile && slot->semaphore == VK_NULL_HANDLE) {
            VkExportSemaphoreCreateInfo exportInfo = {};
            exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
            exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

            VkSemaphoreCreateInfo semInfo = {};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semInfo.pNext = &exportInfo;

            res = api.vkCreateSemaphore(device, &semInfo, nullptr, &slot->semaphore);
            if (res != VK_SUCCESS) {
                qCWarning(lcWlRenderHelper)
                    << subject << phase
                    << "failed: vkCreateSemaphore"
                    << vkResultName(res) << int(res);
                return false;
            }
        }

        return true;
    }
};

Q_GLOBAL_STATIC(QMutex, s_vulkanInteropCommandMutex)
Q_GLOBAL_STATIC(QVector<VulkanInteropCommandContext *>, s_vulkanInteropCommandContexts)

static VulkanInteropCommandContext *vulkanInteropCommandContextFor(VkDevice device,
                                                                   VkQueue queue,
                                                                   uint32_t queueFamilyIndex,
                                                                   bool needSyncFileExport,
                                                                   const char *subject,
                                                                   const char *phase)
{
    for (auto *context : std::as_const(*s_vulkanInteropCommandContexts)) {
        if (context->matches(device, queue, queueFamilyIndex)) {
            if (needSyncFileExport && !context->ensureSyncFileExportFunctions(subject, phase))
                return nullptr;
            return context;
        }
    }

    auto context = std::make_unique<VulkanInteropCommandContext>();
    if (!context->init(device, queue, queueFamilyIndex, needSyncFileExport, subject, phase))
        return nullptr;

    auto result = context.release();
    s_vulkanInteropCommandContexts->append(result);
    return result;
}

static void flushPendingVulkanCommandCleanups(bool wait = false)
{
    QMutexLocker locker(s_vulkanInteropCommandMutex());
    for (auto *context : std::as_const(*s_vulkanInteropCommandContexts))
        context->retireCompletedSlots(wait);
}

static void destroyVulkanInteropCommandContexts()
{
    QVector<VulkanInteropCommandContext *> contexts;
    {
        QMutexLocker locker(s_vulkanInteropCommandMutex());
        contexts.swap(*s_vulkanInteropCommandContexts);
    }

    for (auto *context : std::as_const(contexts)) {
        context->destroy(true);
        delete context;
    }
}

static void destroyVulkanImportedRenderTarget(VulkanImportedRenderTarget *target)
{
    if (!target || target->device == VK_NULL_HANDLE) {
        if (target)
            *target = {};
        return;
    }

    flushPendingVulkanCommandCleanups(true);
    destroyVulkanInteropCommandContexts();

    auto vkGetDeviceProcAddr = resolveVkGetDeviceProcAddr();
    if (!vkGetDeviceProcAddr) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output target cleanup skipped: vkGetDeviceProcAddr unavailable"
            << "image" << Qt::hex << vulkanHandleToInteger(target->image) << Qt::dec
            << "memoryCount" << target->memoryCount;
        *target = {};
        return;
    }

    auto vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        vkGetDeviceProcAddr(target->device, "vkDestroyImage"));
    auto vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
        vkGetDeviceProcAddr(target->device, "vkFreeMemory"));

    if (target->image != VK_NULL_HANDLE && vkDestroyImage)
        vkDestroyImage(target->device, target->image, nullptr);

    if (vkFreeMemory) {
        for (uint32_t i = 0; i < target->memoryCount && i < WLR_DMABUF_MAX_PLANES; ++i) {
            if (target->memories[i] != VK_NULL_HANDLE)
                vkFreeMemory(target->device, target->memories[i], nullptr);
        }
    } else if (target->memoryCount > 0) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output target cleanup could not free imported memory:"
            << "vkFreeMemory unavailable"
            << "image" << Qt::hex << vulkanHandleToInteger(target->image) << Qt::dec
            << "memoryCount" << target->memoryCount;
    }

    qCDebug(lcWlRenderHelper)
        << "Destroyed Vulkan RHI output render target import"
        << "image" << Qt::hex << vulkanHandleToInteger(target->image) << Qt::dec
        << "size" << target->size
        << "format" << drmFormatToName(target->drmFormat)
        << "modifier" << drmModifierToName(target->drmModifier);

    *target = {};
}

static void destroyVulkanImportedNativeTexture(VulkanImportedNativeTexture *texture)
{
    if (!texture || texture->device == VK_NULL_HANDLE) {
        if (texture)
            *texture = {};
        return;
    }

    flushPendingVulkanCommandCleanups(true);
    destroyVulkanInteropCommandContexts();

    auto vkGetDeviceProcAddr = resolveVkGetDeviceProcAddr();
    if (!vkGetDeviceProcAddr) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture cleanup skipped: vkGetDeviceProcAddr unavailable"
            << "image" << Qt::hex << vulkanHandleToInteger(texture->image) << Qt::dec
            << "memoryCount" << texture->memoryCount;
        *texture = {};
        return;
    }

    auto vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        vkGetDeviceProcAddr(texture->device, "vkDestroyImage"));
    auto vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
        vkGetDeviceProcAddr(texture->device, "vkFreeMemory"));

    if (texture->image != VK_NULL_HANDLE && vkDestroyImage)
        vkDestroyImage(texture->device, texture->image, nullptr);

    if (vkFreeMemory) {
        for (uint32_t i = 0; i < texture->memoryCount && i < WLR_DMABUF_MAX_PLANES; ++i) {
            if (texture->memories[i] != VK_NULL_HANDLE)
                vkFreeMemory(texture->device, texture->memories[i], nullptr);
        }
    } else if (texture->memoryCount > 0) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture cleanup could not free imported memory:"
            << "vkFreeMemory unavailable"
            << "image" << Qt::hex << vulkanHandleToInteger(texture->image) << Qt::dec
            << "memoryCount" << texture->memoryCount;
    }

    qCDebug(lcWlRenderHelper)
        << "Destroyed Vulkan RHI client texture import"
        << "image" << Qt::hex << vulkanHandleToInteger(texture->image) << Qt::dec
        << "size" << texture->size
        << "format" << drmFormatToName(texture->drmFormat)
        << "modifier" << drmModifierToName(texture->drmModifier)
        << "lastLayout" << vkImageLayoutName(texture->layout);

    *texture = {};
}

static bool waitDmabufImplicitFence(qw_buffer *buffer, uint32_t flags,
                                    const char *subject, const char *phase)
{
    if (!buffer || !buffer->handle())
        return false;

    wlr_dmabuf_attributes dmabuf = {};
    if (!buffer->get_dmabuf(&dmabuf)) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "cannot wait dmabuf fence: buffer has no dmabuf";
        return false;
    }

    const short events = (flags & DMA_BUF_SYNC_WRITE) ? POLLOUT : POLLIN;
    for (int i = 0; i < dmabuf.n_planes; ++i) {
        pollfd pfd = {};
        pfd.fd = dmabuf.fd[i];
        pfd.events = events;

        const int timeoutMs = 1000;
        const int ret = poll(&pfd, 1, timeoutMs);
        if (ret < 0) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "failed to wait dmabuf fence"
                << "plane" << i
                << "errno" << errno;
            return false;
        }

        if (ret == 0) {
            qCWarning(lcWlRenderHelper)
                << subject << phase
                << "timed out waiting dmabuf fence"
                << "plane" << i
                << "timeoutMs" << timeoutMs;
            return false;
        }
    }

    qCDebug(lcWlRenderHelper)
        << subject << phase
        << "implicit dmabuf fence signaled"
        << "buffer" << buffer
        << "size" << QSize(dmabuf.width, dmabuf.height)
        << "format" << drmFormatToName(dmabuf.format)
        << "modifier" << drmModifierToName(dmabuf.modifier)
        << "planes" << dmabuf.n_planes
        << "events" << events;

    return true;
}

static bool waitSyncFileFence(int syncFileFd, const char *subject, const char *phase)
{
    if (syncFileFd < 0)
        return false;

    pollfd pfd = {};
    pfd.fd = syncFileFd;
    pfd.events = POLLIN;

    const int timeoutMs = 1000;
    int ret = 0;
    do {
        ret = poll(&pfd, 1, timeoutMs);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed to wait sync_file fence"
            << "errno" << errno;
        return false;
    }

    if (ret == 0) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "timed out waiting sync_file fence"
            << "timeoutMs" << timeoutMs;
        return false;
    }

    if (!(pfd.revents & POLLIN)) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "sync_file fence wait returned unexpected events"
            << "events" << pfd.revents;
        return false;
    }

    return true;
}

static bool waitSurfaceExplicitAcquireFence(wlr_surface *surface,
                                            const char *subject,
                                            bool *usedExplicitAcquire)
{
    if (usedExplicitAcquire)
        *usedExplicitAcquire = false;

    if (!surface)
        return true;

    auto *state = qw_linux_drm_syncobj_surface_v1_state::get_surface_state(surface);
    if (!state || !state->has_acquire_timeline())
        return true;

    if (usedExplicitAcquire)
        *usedExplicitAcquire = true;

    const uint64_t acquirePoint = state->acquire_point();
    const int syncFileFd = state->export_acquire_sync_file();
    if (syncFileFd < 0) {
        qCWarning(lcWlRenderHelper)
            << subject << "explicit acquire"
            << "failed to export syncobj timeline point as sync_file"
            << "surface" << surface
            << "point" << acquirePoint;
        return false;
    }

    const bool ok = waitSyncFileFence(syncFileFd, subject, "explicit acquire");
    close(syncFileFd);

    if (ok) {
        qCDebug(lcWlRenderHelper)
            << subject << "explicit acquire fence signaled"
            << "surface" << surface
            << "point" << acquirePoint;
    }

    return ok;
}

static bool importSyncFileIntoDmabuf(qw_buffer *buffer, int syncFileFd, const char *phase)
{
    if (!buffer || !buffer->handle() || syncFileFd < 0)
        return false;

    wlr_dmabuf_attributes dmabuf = {};
    if (!buffer->get_dmabuf(&dmabuf)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output" << phase
            << "cannot import sync_file: output buffer has no dmabuf";
        return false;
    }

    for (int i = 0; i < dmabuf.n_planes; ++i) {
        struct dma_buf_import_sync_file data = {};
        data.flags = DMA_BUF_SYNC_WRITE;
        data.fd = syncFileFd;

        if (ioctl(dmabuf.fd[i], DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &data) != 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output" << phase
                << "failed to import sync_file into dmabuf"
                << "plane" << i
                << "errno" << errno;
            return false;
        }
    }

    return true;
}

static bool submitVulkanImageBarrier(QRhi *rhi,
                                     VkDevice expectedDevice,
                                     VkImage image,
                                     qw_buffer *buffer,
                                     const char *subject,
                                     const char *phase,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     uint32_t srcQueueFamily,
                                     uint32_t dstQueueFamily,
                                     VkAccessFlags srcAccessMask,
                                     VkAccessFlags dstAccessMask,
                                     VkPipelineStageFlags srcStageMask,
                                     VkPipelineStageFlags dstStageMask,
                                     bool exportSyncFile)
{
    if (!rhi || expectedDevice == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
        return false;

    const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!handles || !handles->dev || !handles->gfxQueue || !handles->inst) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed: QRhi Vulkan native handles unavailable";
        return false;
    }

    if (handles->dev != expectedDevice) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed: QRhi device differs from imported Vulkan image"
            << "rhi" << Qt::hex << vulkanHandleToInteger(handles->dev)
            << "expected" << vulkanHandleToInteger(expectedDevice) << Qt::dec;
        return false;
    }

    const uint32_t queueFamily = static_cast<uint32_t>(handles->gfxQueueFamilyIdx);
    QMutexLocker locker(s_vulkanInteropCommandMutex());
    auto *context = vulkanInteropCommandContextFor(handles->dev,
                                                   handles->gfxQueue,
                                                   queueFamily,
                                                   exportSyncFile,
                                                   subject,
                                                   phase);
    if (!context)
        return false;

    auto *slot = context->acquireSlot(exportSyncFile, subject, phase);
    if (!slot)
        return false;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult res = context->api.vkBeginCommandBuffer(slot->commandBuffer, &beginInfo);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed: vkBeginCommandBuffer"
            << vkResultName(res) << int(res);
        return false;
    }

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    barrier.image = image;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    context->api.vkCmdPipelineBarrier(slot->commandBuffer,
                                      srcStageMask,
                                      dstStageMask,
                                      0, 0, nullptr, 0, nullptr, 1, &barrier);

    res = context->api.vkEndCommandBuffer(slot->commandBuffer);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed: vkEndCommandBuffer"
            << vkResultName(res) << int(res);
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &slot->commandBuffer;
    if (exportSyncFile) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &slot->semaphore;
    }

    res = context->api.vkQueueSubmit(handles->gfxQueue, 1, &submitInfo, slot->fence);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed: vkQueueSubmit"
            << vkResultName(res) << int(res);
        return false;
    }

    slot->busy = true;

    if (!exportSyncFile)
        return true;

    VkSemaphoreGetFdInfoKHR getFdInfo = {};
    getFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    getFdInfo.semaphore = slot->semaphore;
    getFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

    int syncFileFd = -1;
    res = context->api.vkGetSemaphoreFdKHR(handles->dev, &getFdInfo, &syncFileFd);
    if (res != VK_SUCCESS || syncFileFd < 0) {
        qCWarning(lcWlRenderHelper)
            << subject << phase
            << "failed: vkGetSemaphoreFdKHR"
            << vkResultName(res) << int(res)
            << "fd" << syncFileFd;
        return false;
    }

    const bool ok = importSyncFileIntoDmabuf(buffer, syncFileFd, phase);
    close(syncFileFd);
    return ok;
}

static bool submitVulkanOutputTargetBarrier(QRhi *rhi,
                                            VulkanImportedRenderTarget *target,
                                            qw_buffer *buffer,
                                            const char *phase,
                                            VkImageLayout oldLayout,
                                            VkImageLayout newLayout,
                                            uint32_t srcQueueFamily,
                                            uint32_t dstQueueFamily,
                                            VkAccessFlags srcAccessMask,
                                            VkAccessFlags dstAccessMask,
                                            VkPipelineStageFlags srcStageMask,
                                            VkPipelineStageFlags dstStageMask,
                                            bool exportSyncFile)
{
    if (!rhi || !target || !target->isValid())
        return false;

    const bool ok = submitVulkanImageBarrier(rhi,
                                             target->device,
                                             target->image,
                                             buffer,
                                             "Vulkan RHI output",
                                             phase,
                                             oldLayout,
                                             newLayout,
                                             srcQueueFamily,
                                             dstQueueFamily,
                                             srcAccessMask,
                                             dstAccessMask,
                                             srcStageMask,
                                             dstStageMask,
                                             exportSyncFile);

    if (ok) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI output" << phase
            << "barrier submitted"
            << "image" << Qt::hex << vulkanHandleToInteger(target->image) << Qt::dec
            << "oldLayout" << vkImageLayoutName(oldLayout)
            << "newLayout" << vkImageLayoutName(newLayout)
            << "srcQueue" << srcQueueFamily
            << "dstQueue" << dstQueueFamily
            << "syncFile" << exportSyncFile;
    }

    return ok;
}

// Resolve EGL function pointers for dmabuf import. These are EGL extensions
// (EGL_KHR_image_base, EGL_EXT_image_dma_buf_import, GL_OES_EGL_image) and
// must be resolved at runtime via eglGetProcAddress.
static PFNEGLCREATEIMAGEKHRPROC resolveEglCreateImageKHR()
{
    static auto proc = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    return proc;
}
static PFNEGLDESTROYIMAGEKHRPROC resolveEglDestroyImageKHR()
{
    static auto proc = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    return proc;
}
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC resolveGlEGLImageTargetTexture2DOES()
{
    static auto proc = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    return proc;
}

static void clearGlErrors()
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

static bool textureUploadSucceeded()
{
    return glGetError() == GL_NO_ERROR;
}

static bool drmFormatLikelyHasAlpha(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_RGB332:
    case DRM_FORMAT_BGR233:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
#ifdef DRM_FORMAT_XRGB16161616
    case DRM_FORMAT_XRGB16161616:
#endif
#ifdef DRM_FORMAT_XBGR16161616
    case DRM_FORMAT_XBGR16161616:
#endif
#ifdef DRM_FORMAT_XRGB16161616F
    case DRM_FORMAT_XRGB16161616F:
#endif
#ifdef DRM_FORMAT_XBGR16161616F
    case DRM_FORMAT_XBGR16161616F:
#endif
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_XYUV8888:
    case DRM_FORMAT_XVUY8888:
        return false;
    default:
        return true;
    }
}

static bool surfaceOpaqueRegionCoversBuffer(wlr_surface *surface, qw_buffer *buffer)
{
    if (!surface || !buffer || !buffer->handle())
        return false;

    if (!pixman_region32_not_empty(&surface->opaque_region))
        return false;

    const int width = surface->current.width > 0
        ? surface->current.width
        : buffer->handle()->width;
    const int height = surface->current.height > 0
        ? surface->current.height
        : buffer->handle()->height;
    if (width <= 0 || height <= 0)
        return false;

    const pixman_box32_t *box = pixman_region32_extents(&surface->opaque_region);
    return box
        && box->x1 <= 0
        && box->y1 <= 0
        && box->x2 >= width
        && box->y2 >= height;
}

Q_GLOBAL_STATIC(QMutex, s_pendingNativeTextureCleanupMutex)
Q_GLOBAL_STATIC(QVector<WRenderHelper::NativeTextureCleanup>, s_pendingNativeTextureCleanups)

static bool releaseNativeTextureNow(WRenderHelper::NativeTextureCleanup *cleanup)
{
    if (!cleanup || cleanup->type == WRenderHelper::NativeTextureCleanup::Type::None)
        return true;

    if (cleanup->type == WRenderHelper::NativeTextureCleanup::Type::OpenGLTexture) {
        if (!cleanup->texture && !cleanup->eglImage) {
            *cleanup = {};
            return true;
        }

        if (!QOpenGLContext::currentContext())
            return false;

        if (cleanup->texture) {
            GLuint texture = GLuint(cleanup->texture);
            glDeleteTextures(1, &texture);
        }

        if (cleanup->eglImage && cleanup->eglDisplay) {
            if (auto destroyImage = resolveEglDestroyImageKHR())
                destroyImage(reinterpret_cast<EGLDisplay>(cleanup->eglDisplay),
                             reinterpret_cast<EGLImage>(cleanup->eglImage));
        }
    } else if (cleanup->type == WRenderHelper::NativeTextureCleanup::Type::VulkanTexture) {
        auto imported = static_cast<VulkanImportedNativeTexture *>(cleanup->nativeData);
        if (imported) {
            destroyVulkanImportedNativeTexture(imported);
            delete imported;
        }
    } else if (cleanup->type == WRenderHelper::NativeTextureCleanup::Type::VulkanRenderTarget) {
        auto imported = static_cast<VulkanImportedRenderTarget *>(cleanup->nativeData);
        if (imported) {
            destroyVulkanImportedRenderTarget(imported);
            delete imported;
        }
    }

    *cleanup = {};
    return true;
}

static void queueNativeTextureCleanup(WRenderHelper::NativeTextureCleanup *cleanup)
{
    if (!cleanup || cleanup->type == WRenderHelper::NativeTextureCleanup::Type::None)
        return;

    QMutexLocker locker(s_pendingNativeTextureCleanupMutex());
    s_pendingNativeTextureCleanups->append(*cleanup);
    *cleanup = {};
}

static void flushPendingNativeTextureCleanups()
{
    if (!QOpenGLContext::currentContext())
        return;

    QVector<WRenderHelper::NativeTextureCleanup> pending;
    {
        QMutexLocker locker(s_pendingNativeTextureCleanupMutex());
        if (s_pendingNativeTextureCleanups->isEmpty())
            return;
        pending.swap(*s_pendingNativeTextureCleanups);
    }

    QVector<WRenderHelper::NativeTextureCleanup> remaining;
    for (auto &cleanup : pending) {
        if (!releaseNativeTextureNow(&cleanup))
            remaining.append(cleanup);
    }

    if (!remaining.isEmpty()) {
        QMutexLocker locker(s_pendingNativeTextureCleanupMutex());
        *s_pendingNativeTextureCleanups += remaining;
    }
}

void BufferData::destroyEglDmabufTexture()
{
    WRenderHelper::NativeTextureCleanup cleanup {
        glTexture || eglImage != EGL_NO_IMAGE
            ? WRenderHelper::NativeTextureCleanup::Type::OpenGLTexture
            : WRenderHelper::NativeTextureCleanup::Type::None,
        glTexture,
        eglImage != EGL_NO_IMAGE ? reinterpret_cast<void *>(eglImage) : nullptr,
        eglDisplay != EGL_NO_DISPLAY ? reinterpret_cast<void *>(eglDisplay) : nullptr,
    };
    WRenderHelper::releaseNativeTexture(&cleanup);
    glTexture = 0;
    eglImage = EGL_NO_IMAGE;
    eglDisplay = EGL_NO_DISPLAY;
}

void BufferData::destroyVulkanRenderTarget()
{
    destroyVulkanImportedRenderTarget(&vulkanRenderTarget);
}

static VkFormat vkFormatFromDrmFormat(uint32_t drmFormat)
{
    switch (drmFormat) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_RGB565:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case DRM_FORMAT_BGR565:
        return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_ARGB2101010:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_ABGR2101010:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
#ifdef DRM_FORMAT_XBGR16161616
    case DRM_FORMAT_XBGR16161616:
#endif
#ifdef DRM_FORMAT_ABGR16161616
    case DRM_FORMAT_ABGR16161616:
#endif
#if defined(DRM_FORMAT_XBGR16161616) || defined(DRM_FORMAT_ABGR16161616)
        return VK_FORMAT_R16G16B16A16_UNORM;
#endif
#ifdef DRM_FORMAT_XBGR16161616F
    case DRM_FORMAT_XBGR16161616F:
#endif
#ifdef DRM_FORMAT_ABGR16161616F
    case DRM_FORMAT_ABGR16161616F:
#endif
#if defined(DRM_FORMAT_XBGR16161616F) || defined(DRM_FORMAT_ABGR16161616F)
        return VK_FORMAT_R16G16B16A16_SFLOAT;
#endif
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

static VkImageAspectFlagBits vulkanMemoryPlaneAspect(uint32_t plane)
{
    switch (plane) {
    case 0: return VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
    case 1: return VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;
    case 2: return VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT;
    case 3: return VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT;
    default: return VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
    }
}

static bool dmabufUsesDisjointMemory(const wlr_dmabuf_attributes *attribs)
{
    if (!attribs || attribs->n_planes <= 1)
        return false;

    struct stat firstStat = {};
    if (fstat(attribs->fd[0], &firstStat) != 0) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import: fstat failed for dmabuf plane 0"
            << "errno" << errno;
        return true;
    }

    for (int i = 1; i < attribs->n_planes; ++i) {
        struct stat planeStat = {};
        if (fstat(attribs->fd[i], &planeStat) != 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output import: fstat failed for dmabuf plane"
                << i << "errno" << errno;
            return true;
        }

        if (firstStat.st_ino != planeStat.st_ino)
            return true;
    }

    return false;
}

static int findVulkanMemoryType(PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties,
                                VkPhysicalDevice physicalDevice,
                                VkMemoryPropertyFlags flags,
                                uint32_t requiredBits)
{
    if (!vkGetPhysicalDeviceMemoryProperties || physicalDevice == VK_NULL_HANDLE)
        return -1;

    VkPhysicalDeviceMemoryProperties props = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);

    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        const bool typeMatches = requiredBits & (1u << i);
        const bool flagsMatch = (props.memoryTypes[i].propertyFlags & flags) == flags;
        if (typeMatches && flagsMatch)
            return int(i);
    }

    return -1;
}

struct Q_DECL_HIDDEN VulkanDmabufTextureImportSupport {
    VkFormat sampledViewFormat = VK_FORMAT_UNDEFINED;

    bool usesMutableSrgbView() const {
        return sampledViewFormat != VK_FORMAT_UNDEFINED;
    }
};

static VkResult queryVulkanDmabufTextureImageFormatProperties(
    PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2,
    VkPhysicalDevice physicalDevice,
    VkFormat vkFormat,
    VkFormat srgbViewFormat,
    const wlr_dmabuf_attributes *attribs,
    bool disjoint,
    VkImageFormatProperties *imageFormatProperties,
    VkExternalMemoryFeatureFlags *externalMemoryFeatures)
{
    if (!vkGetPhysicalDeviceImageFormatProperties2 || physicalDevice == VK_NULL_HANDLE
        || !attribs || !imageFormatProperties || !externalMemoryFeatures) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkFormat viewFormats[2] = { vkFormat, srgbViewFormat };
    VkImageFormatListCreateInfoKHR formatListInfo = {};
    formatListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
    formatListInfo.viewFormatCount = srgbViewFormat != VK_FORMAT_UNDEFINED ? 2u : 1u;
    formatListInfo.pViewFormats = viewFormats;

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = {};
    modifierInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
    modifierInfo.drmFormatModifier = attribs->modifier;
    modifierInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (srgbViewFormat != VK_FORMAT_UNDEFINED)
        modifierInfo.pNext = &formatListInfo;

    VkPhysicalDeviceExternalImageFormatInfo externalInfo = {};
    externalInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    externalInfo.pNext = &modifierInfo;

    VkPhysicalDeviceImageFormatInfo2 formatInfo = {};
    formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    formatInfo.type = VK_IMAGE_TYPE_2D;
    formatInfo.format = vkFormat;
    formatInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    formatInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    formatInfo.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
    if (srgbViewFormat != VK_FORMAT_UNDEFINED)
        formatInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    formatInfo.pNext = &externalInfo;

    VkExternalImageFormatProperties externalProps = {};
    externalProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 imageProps = {};
    imageProps.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    imageProps.pNext = &externalProps;

    const VkResult res =
        vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, &formatInfo, &imageProps);
    if (res == VK_SUCCESS) {
        *imageFormatProperties = imageProps.imageFormatProperties;
        *externalMemoryFeatures = externalProps.externalMemoryProperties.externalMemoryFeatures;
    }
    return res;
}

static bool vulkanImagePropertiesFitDmabuf(const VkImageFormatProperties &imageFormatProperties,
                                           const wlr_dmabuf_attributes *attribs)
{
    return attribs
        && uint32_t(attribs->width) <= imageFormatProperties.maxExtent.width
        && uint32_t(attribs->height) <= imageFormatProperties.maxExtent.height;
}

static VkFormatFeatureFlags vulkanRequiredFeaturesForImageUsage(VkImageUsageFlags usage)
{
    VkFormatFeatureFlags features = 0;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        features |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        features |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
        features |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    return features;
}

static VkImageUsageFlags vulkanImageUsageForRhiTextureFlags(QRhiTexture::Flags flags)
{
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (flags.testFlag(QRhiTexture::RenderTarget))
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (flags.testFlag(QRhiTexture::UsedAsTransferSource)
        || flags.testFlag(QRhiTexture::UsedWithGenerateMips)) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (flags.testFlag(QRhiTexture::UsedWithLoadStore))
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    return usage;
}

static bool validateVulkanDmabufRenderImportSupport(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkFormat vkFormat,
    const wlr_dmabuf_attributes *attribs,
    bool disjoint,
    VkImageUsageFlags imageUsage)
{
    if (!attribs)
        return false;

    if (attribs->modifier == DRM_FORMAT_MOD_INVALID) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: dmabuf modifier is INVALID"
            << "format" << drmFormatToName(attribs->format)
            << "size" << QSize(attribs->width, attribs->height);
        return false;
    }

    auto vkGetInstanceProcAddr = resolveVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr || instance == VK_NULL_HANDLE) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: vkGetInstanceProcAddr or VkInstance unavailable";
        return false;
    }

    auto vkGetPhysicalDeviceImageFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2"));
    auto vkGetPhysicalDeviceFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2"));

    if (!vkGetPhysicalDeviceImageFormatProperties2) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: vkGetPhysicalDeviceImageFormatProperties2 unavailable";
        return false;
    }

    if (vkGetPhysicalDeviceFormatProperties2) {
        VkDrmFormatModifierPropertiesListEXT modifierList = {};
        modifierList.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

        VkFormatProperties2 formatProps = {};
        formatProps.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        formatProps.pNext = &modifierList;
        vkGetPhysicalDeviceFormatProperties2(physicalDevice, vkFormat, &formatProps);

        QVector<VkDrmFormatModifierPropertiesEXT> modifiers(
            int(modifierList.drmFormatModifierCount));
        if (!modifiers.isEmpty()) {
            modifierList.pDrmFormatModifierProperties = modifiers.data();
            vkGetPhysicalDeviceFormatProperties2(physicalDevice, vkFormat, &formatProps);

            const auto found = std::find_if(modifiers.cbegin(), modifiers.cend(),
                                            [attribs](const VkDrmFormatModifierPropertiesEXT &props) {
                return props.drmFormatModifier == attribs->modifier;
            });

            if (found == modifiers.cend()) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: modifier not exposed for VkFormat"
                    << vkFormat
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier);
                return false;
            }

            if (int(found->drmFormatModifierPlaneCount) != attribs->n_planes) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: modifier plane count mismatch"
                    << "expected" << found->drmFormatModifierPlaneCount
                    << "dmabuf" << attribs->n_planes
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier);
                return false;
            }

            const VkFormatFeatureFlags requiredFeatures =
                vulkanRequiredFeaturesForImageUsage(imageUsage);
            if ((found->drmFormatModifierTilingFeatures & requiredFeatures)
                != requiredFeatures) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: modifier lacks required image usage support"
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier)
                    << "usage" << Qt::hex << imageUsage
                    << "requiredFeatures" << requiredFeatures
                    << "features" << Qt::hex << found->drmFormatModifierTilingFeatures << Qt::dec;
                return false;
            }

            if (disjoint
                && !(found->drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT)) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: disjoint dmabuf lacks DISJOINT support"
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier)
                    << "features" << Qt::hex << found->drmFormatModifierTilingFeatures << Qt::dec;
                return false;
            }
        }
    }

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = {};
    modifierInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
    modifierInfo.drmFormatModifier = attribs->modifier;
    modifierInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkPhysicalDeviceExternalImageFormatInfo externalInfo = {};
    externalInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    externalInfo.pNext = &modifierInfo;

    VkPhysicalDeviceImageFormatInfo2 formatInfo = {};
    formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    formatInfo.type = VK_IMAGE_TYPE_2D;
    formatInfo.format = vkFormat;
    formatInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    formatInfo.usage = imageUsage;
    formatInfo.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
    formatInfo.pNext = &externalInfo;

    VkExternalImageFormatProperties externalProps = {};
    externalProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 imageProps = {};
    imageProps.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    imageProps.pNext = &externalProps;

    VkResult res = vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, &formatInfo, &imageProps);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: image format properties query failed"
            << vkResultName(res) << int(res)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "vkFormat" << vkFormat
            << "size" << QSize(attribs->width, attribs->height);
        return false;
    }

    if (!(externalProps.externalMemoryProperties.externalMemoryFeatures
          & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: external memory is not importable"
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "vkFormat" << vkFormat;
        return false;
    }

    if (uint32_t(attribs->width) > imageProps.imageFormatProperties.maxExtent.width
        || uint32_t(attribs->height) > imageProps.imageFormatProperties.maxExtent.height) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: dmabuf exceeds max image extent"
            << "size" << QSize(attribs->width, attribs->height)
            << "max" << QSize(imageProps.imageFormatProperties.maxExtent.width,
                              imageProps.imageFormatProperties.maxExtent.height)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier);
        return false;
    }

    return true;
}

static bool importDmabufAsVulkanRenderTarget(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const wlr_dmabuf_attributes *attribs,
    VkImageUsageFlags imageUsage,
    bool ownerIsForeign,
    VulkanImportedRenderTarget *out)
{
    if (!attribs || !out || instance == VK_NULL_HANDLE
        || physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: missing Vulkan handles or dmabuf";
        return false;
    }

    if (attribs->n_planes <= 0 || attribs->n_planes > WLR_DMABUF_MAX_PLANES) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: invalid dmabuf plane count"
            << attribs->n_planes;
        return false;
    }

    for (int i = 0; i < attribs->n_planes; ++i) {
        if (attribs->fd[i] < 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output import rejected: invalid dmabuf fd on plane"
                << i;
            return false;
        }
    }

    const VkFormat vkFormat = vkFormatFromDrmFormat(attribs->format);
    if (vkFormat == VK_FORMAT_UNDEFINED) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: unsupported DRM format"
            << drmFormatToName(attribs->format)
            << Qt::hex << attribs->format << Qt::dec;
        return false;
    }

    const bool disjoint = dmabufUsesDisjointMemory(attribs);
    if (!validateVulkanDmabufRenderImportSupport(instance, physicalDevice, vkFormat,
                                                 attribs, disjoint, imageUsage)) {
        return false;
    }

    auto vkGetDeviceProcAddr = resolveVkGetDeviceProcAddr();
    auto vkGetInstanceProcAddr = resolveVkGetInstanceProcAddr();
    if (!vkGetDeviceProcAddr || !vkGetInstanceProcAddr) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: Vulkan loader entry points unavailable";
        return false;
    }

    auto vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
        vkGetDeviceProcAddr(device, "vkCreateImage"));
    auto vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        vkGetDeviceProcAddr(device, "vkDestroyImage"));
    auto vkGetMemoryFdPropertiesKHR = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
        vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
    auto vkGetImageMemoryRequirements2 = reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
        vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements2"));
    auto vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
        vkGetDeviceProcAddr(device, "vkAllocateMemory"));
    auto vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
        vkGetDeviceProcAddr(device, "vkFreeMemory"));
    auto vkBindImageMemory2 = reinterpret_cast<PFN_vkBindImageMemory2>(
        vkGetDeviceProcAddr(device, "vkBindImageMemory2"));
    auto vkGetPhysicalDeviceMemoryProperties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));

    if (!vkCreateImage || !vkDestroyImage || !vkGetMemoryFdPropertiesKHR
        || !vkGetImageMemoryRequirements2 || !vkAllocateMemory || !vkFreeMemory
        || !vkBindImageMemory2 || !vkGetPhysicalDeviceMemoryProperties) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import rejected: required Vulkan import functions unavailable";
        return false;
    }

    VkExternalMemoryImageCreateInfo externalImageInfo = {};
    externalImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkSubresourceLayout planeLayouts[WLR_DMABUF_MAX_PLANES] = {};
    for (int i = 0; i < attribs->n_planes; ++i) {
        planeLayouts[i].offset = attribs->offset[i];
        planeLayouts[i].rowPitch = attribs->stride[i];
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT modifierInfo = {};
    modifierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
    modifierInfo.drmFormatModifier = attribs->modifier;
    modifierInfo.drmFormatModifierPlaneCount = uint32_t(attribs->n_planes);
    modifierInfo.pPlaneLayouts = planeLayouts;
    externalImageInfo.pNext = &modifierInfo;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalImageInfo;
    imageInfo.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent = VkExtent3D { uint32_t(attribs->width), uint32_t(attribs->height), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    imageInfo.usage = imageUsage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VulkanImportedRenderTarget imported;
    imported.device = device;
    imported.format = vkFormat;
    imported.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    imported.ownerIsForeign = ownerIsForeign;
    imported.scanoutReleaseReady = false;
    imported.size = QSize(attribs->width, attribs->height);
    imported.drmFormat = attribs->format;
    imported.drmModifier = attribs->modifier;

    VkResult res = vkCreateImage(device, &imageInfo, nullptr, &imported.image);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import failed: vkCreateImage"
            << vkResultName(res) << int(res)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "vkFormat" << vkFormat
            << "size" << imported.size
            << "planes" << attribs->n_planes
            << "disjoint" << disjoint;
        return false;
    }

    const uint32_t memoryCount = disjoint ? uint32_t(attribs->n_planes) : 1u;
    VkBindImageMemoryInfo bindInfos[WLR_DMABUF_MAX_PLANES] = {};
    VkBindImagePlaneMemoryInfo planeInfos[WLR_DMABUF_MAX_PLANES] = {};

    for (uint32_t i = 0; i < memoryCount; ++i) {
        VkMemoryFdPropertiesKHR fdProps = {};
        fdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
        res = vkGetMemoryFdPropertiesKHR(device,
                                         VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                         attribs->fd[i],
                                         &fdProps);
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output import failed: vkGetMemoryFdPropertiesKHR"
                << vkResultName(res) << int(res)
                << "plane" << i
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedRenderTarget(&imported);
            return false;
        }

        VkImagePlaneMemoryRequirementsInfo planeReqInfo = {};
        VkImageMemoryRequirementsInfo2 memoryReqInfo = {};
        memoryReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memoryReqInfo.image = imported.image;

        if (disjoint) {
            planeReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
            planeReqInfo.planeAspect = vulkanMemoryPlaneAspect(i);
            memoryReqInfo.pNext = &planeReqInfo;
        }

        VkMemoryRequirements2 memoryReq = {};
        memoryReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        vkGetImageMemoryRequirements2(device, &memoryReqInfo, &memoryReq);

        const int memoryType = findVulkanMemoryType(
            vkGetPhysicalDeviceMemoryProperties,
            physicalDevice,
            0,
            memoryReq.memoryRequirements.memoryTypeBits & fdProps.memoryTypeBits);
        if (memoryType < 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output import failed: no compatible memory type"
                << "plane" << i
                << "requirements" << Qt::hex << memoryReq.memoryRequirements.memoryTypeBits
                << "fd" << fdProps.memoryTypeBits << Qt::dec
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedRenderTarget(&imported);
            return false;
        }

        const int dupFd = fcntl(attribs->fd[i], F_DUPFD_CLOEXEC, 0);
        if (dupFd < 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output import failed: failed to duplicate dmabuf fd"
                << "plane" << i
                << "errno" << errno
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedRenderTarget(&imported);
            return false;
        }

        VkImportMemoryFdInfoKHR importFdInfo = {};
        importFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        importFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        importFdInfo.fd = dupFd;

        VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
        dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedInfo.image = imported.image;
        importFdInfo.pNext = &dedicatedInfo;

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = &importFdInfo;
        allocInfo.allocationSize = memoryReq.memoryRequirements.size;
        allocInfo.memoryTypeIndex = uint32_t(memoryType);

        res = vkAllocateMemory(device, &allocInfo, nullptr, &imported.memories[i]);
        if (res != VK_SUCCESS) {
            close(dupFd);
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output import failed: vkAllocateMemory"
                << vkResultName(res) << int(res)
                << "plane" << i
                << "allocationSize" << qulonglong(allocInfo.allocationSize)
                << "memoryType" << memoryType
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedRenderTarget(&imported);
            return false;
        }
        ++imported.memoryCount;

        bindInfos[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
        bindInfos[i].image = imported.image;
        bindInfos[i].memory = imported.memories[i];
        bindInfos[i].memoryOffset = 0;

        if (disjoint) {
            planeInfos[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
            planeInfos[i].planeAspect = vulkanMemoryPlaneAspect(i);
            bindInfos[i].pNext = &planeInfos[i];
        }
    }

    res = vkBindImageMemory2(device, memoryCount, bindInfos);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output import failed: vkBindImageMemory2"
            << vkResultName(res) << int(res)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "planes" << attribs->n_planes
            << "memoryCount" << memoryCount
            << "disjoint" << disjoint;
        destroyVulkanImportedRenderTarget(&imported);
        return false;
    }

    qCInfo(lcWlRenderHelper)
        << "Vulkan RHI output import succeeded"
        << "image" << Qt::hex << vulkanHandleToInteger(imported.image) << Qt::dec
        << "size" << imported.size
        << "format" << drmFormatToName(imported.drmFormat)
        << "modifier" << drmModifierToName(imported.drmModifier)
        << "vkFormat" << imported.format
        << "planes" << attribs->n_planes
        << "memoryCount" << imported.memoryCount
        << "disjoint" << disjoint;

    *out = imported;
    return true;
}

static bool validateVulkanDmabufTextureImportSupport(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkFormat vkFormat,
    const wlr_dmabuf_attributes *attribs,
    bool disjoint,
    VulkanDmabufTextureImportSupport *support)
{
    if (!attribs)
        return false;
    if (support)
        *support = {};

    if (attribs->modifier == DRM_FORMAT_MOD_INVALID) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: dmabuf modifier is INVALID"
            << "format" << drmFormatToName(attribs->format)
            << "size" << QSize(attribs->width, attribs->height);
        return false;
    }

    auto vkGetInstanceProcAddr = resolveVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr || instance == VK_NULL_HANDLE) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: vkGetInstanceProcAddr or VkInstance unavailable";
        return false;
    }

    auto vkGetPhysicalDeviceImageFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2"));
    auto vkGetPhysicalDeviceFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2"));

    if (!vkGetPhysicalDeviceImageFormatProperties2) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: vkGetPhysicalDeviceImageFormatProperties2 unavailable";
        return false;
    }

    if (vkGetPhysicalDeviceFormatProperties2) {
        VkDrmFormatModifierPropertiesListEXT modifierList = {};
        modifierList.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

        VkFormatProperties2 formatProps = {};
        formatProps.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        formatProps.pNext = &modifierList;
        vkGetPhysicalDeviceFormatProperties2(physicalDevice, vkFormat, &formatProps);

        QVector<VkDrmFormatModifierPropertiesEXT> modifiers(
            int(modifierList.drmFormatModifierCount));
        if (!modifiers.isEmpty()) {
            modifierList.pDrmFormatModifierProperties = modifiers.data();
            vkGetPhysicalDeviceFormatProperties2(physicalDevice, vkFormat, &formatProps);

            const auto found = std::find_if(modifiers.cbegin(), modifiers.cend(),
                                            [attribs](const VkDrmFormatModifierPropertiesEXT &props) {
                return props.drmFormatModifier == attribs->modifier;
            });

            if (found == modifiers.cend()) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI client texture import rejected: modifier not exposed for VkFormat"
                    << vkFormat
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier);
                return false;
            }

            if (int(found->drmFormatModifierPlaneCount) != attribs->n_planes) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI client texture import rejected: modifier plane count mismatch"
                    << "expected" << found->drmFormatModifierPlaneCount
                    << "dmabuf" << attribs->n_planes
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier);
                return false;
            }

            constexpr VkFormatFeatureFlags requiredFeatures =
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            if ((found->drmFormatModifierTilingFeatures & requiredFeatures) != requiredFeatures) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI client texture import rejected: modifier lacks sampled texture support"
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier)
                    << "features" << Qt::hex << found->drmFormatModifierTilingFeatures << Qt::dec;
                return false;
            }

            if (disjoint
                && !(found->drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT)) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI client texture import rejected: disjoint dmabuf lacks DISJOINT support"
                    << "format" << drmFormatToName(attribs->format)
                    << "modifier" << drmModifierToName(attribs->modifier)
                    << "features" << Qt::hex << found->drmFormatModifierTilingFeatures << Qt::dec;
                return false;
            }
        }
    }

    VkImageFormatProperties imageFormatProperties = {};
    VkExternalMemoryFeatureFlags externalMemoryFeatures = 0;
    VkResult res = VK_SUCCESS;

    if (support)
        support->sampledViewFormat = VK_FORMAT_UNDEFINED;

    res = queryVulkanDmabufTextureImageFormatProperties(
        vkGetPhysicalDeviceImageFormatProperties2,
        physicalDevice,
        vkFormat,
        VK_FORMAT_UNDEFINED,
        attribs,
        disjoint,
        &imageFormatProperties,
        &externalMemoryFeatures);

    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: image format properties query failed"
            << vkResultName(res) << int(res)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "vkFormat" << vkFormat
            << "size" << QSize(attribs->width, attribs->height);
        return false;
    }

    if (!(externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: external memory is not importable"
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "vkFormat" << vkFormat;
        return false;
    }

    if (!vulkanImagePropertiesFitDmabuf(imageFormatProperties, attribs)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: dmabuf exceeds max image extent"
            << "size" << QSize(attribs->width, attribs->height)
            << "max" << QSize(imageFormatProperties.maxExtent.width,
                              imageFormatProperties.maxExtent.height)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier);
        return false;
    }

    return true;
}

static bool importDmabufAsVulkanNativeTexture(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const wlr_dmabuf_attributes *attribs,
    VulkanImportedNativeTexture *out)
{
    if (!attribs || !out || instance == VK_NULL_HANDLE
        || physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: missing Vulkan handles or dmabuf";
        return false;
    }

    if (attribs->n_planes <= 0 || attribs->n_planes > WLR_DMABUF_MAX_PLANES) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: invalid dmabuf plane count"
            << attribs->n_planes;
        return false;
    }

    for (int i = 0; i < attribs->n_planes; ++i) {
        if (attribs->fd[i] < 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI client texture import rejected: invalid dmabuf fd on plane"
                << i;
            return false;
        }
    }

    const VkFormat vkFormat = vkFormatFromDrmFormat(attribs->format);
    if (vkFormat == VK_FORMAT_UNDEFINED) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: unsupported DRM format"
            << drmFormatToName(attribs->format)
            << Qt::hex << attribs->format << Qt::dec;
        return false;
    }

    const bool disjoint = dmabufUsesDisjointMemory(attribs);
    VulkanDmabufTextureImportSupport importSupport;
    if (!validateVulkanDmabufTextureImportSupport(instance, physicalDevice, vkFormat,
                                                  attribs, disjoint, &importSupport)) {
        return false;
    }
    const bool useMutableSrgbView = importSupport.usesMutableSrgbView();

    auto vkGetDeviceProcAddr = resolveVkGetDeviceProcAddr();
    auto vkGetInstanceProcAddr = resolveVkGetInstanceProcAddr();
    if (!vkGetDeviceProcAddr || !vkGetInstanceProcAddr) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: Vulkan loader entry points unavailable";
        return false;
    }

    auto vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
        vkGetDeviceProcAddr(device, "vkCreateImage"));
    auto vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        vkGetDeviceProcAddr(device, "vkDestroyImage"));
    auto vkGetMemoryFdPropertiesKHR = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
        vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
    auto vkGetImageMemoryRequirements2 = reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
        vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements2"));
    auto vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
        vkGetDeviceProcAddr(device, "vkAllocateMemory"));
    auto vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
        vkGetDeviceProcAddr(device, "vkFreeMemory"));
    auto vkBindImageMemory2 = reinterpret_cast<PFN_vkBindImageMemory2>(
        vkGetDeviceProcAddr(device, "vkBindImageMemory2"));
    auto vkGetPhysicalDeviceMemoryProperties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));

    if (!vkCreateImage || !vkDestroyImage || !vkGetMemoryFdPropertiesKHR
        || !vkGetImageMemoryRequirements2 || !vkAllocateMemory || !vkFreeMemory
        || !vkBindImageMemory2 || !vkGetPhysicalDeviceMemoryProperties) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import rejected: required Vulkan import functions unavailable";
        return false;
    }

    VkExternalMemoryImageCreateInfo externalImageInfo = {};
    externalImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkSubresourceLayout planeLayouts[WLR_DMABUF_MAX_PLANES] = {};
    for (int i = 0; i < attribs->n_planes; ++i) {
        planeLayouts[i].offset = attribs->offset[i];
        planeLayouts[i].rowPitch = attribs->stride[i];
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT modifierInfo = {};
    modifierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
    modifierInfo.drmFormatModifier = attribs->modifier;
    modifierInfo.drmFormatModifierPlaneCount = uint32_t(attribs->n_planes);
    modifierInfo.pPlaneLayouts = planeLayouts;
    VkFormat viewFormats[2] = { vkFormat, importSupport.sampledViewFormat };
    VkImageFormatListCreateInfoKHR formatListInfo = {};
    if (useMutableSrgbView) {
        formatListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
        formatListInfo.viewFormatCount = 2;
        formatListInfo.pViewFormats = viewFormats;
        modifierInfo.pNext = &formatListInfo;
    }
    externalImageInfo.pNext = &modifierInfo;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalImageInfo;
    imageInfo.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
    if (useMutableSrgbView)
        imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent = VkExtent3D { uint32_t(attribs->width), uint32_t(attribs->height), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VulkanImportedNativeTexture imported;
    imported.device = device;
    imported.format = useMutableSrgbView ? importSupport.sampledViewFormat : vkFormat;
    imported.layout = VK_IMAGE_LAYOUT_GENERAL;
    imported.size = QSize(attribs->width, attribs->height);
    imported.drmFormat = attribs->format;
    imported.drmModifier = attribs->modifier;

    VkResult res = vkCreateImage(device, &imageInfo, nullptr, &imported.image);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import failed: vkCreateImage"
            << vkResultName(res) << int(res)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "imageVkFormat" << vkFormat
            << "viewVkFormat" << imported.format
            << "size" << imported.size
            << "planes" << attribs->n_planes
            << "disjoint" << disjoint
            << "mutableSrgbView" << useMutableSrgbView;
        return false;
    }

    const uint32_t memoryCount = disjoint ? uint32_t(attribs->n_planes) : 1u;
    VkBindImageMemoryInfo bindInfos[WLR_DMABUF_MAX_PLANES] = {};
    VkBindImagePlaneMemoryInfo planeInfos[WLR_DMABUF_MAX_PLANES] = {};

    for (uint32_t i = 0; i < memoryCount; ++i) {
        VkMemoryFdPropertiesKHR fdProps = {};
        fdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
        res = vkGetMemoryFdPropertiesKHR(device,
                                         VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                         attribs->fd[i],
                                         &fdProps);
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI client texture import failed: vkGetMemoryFdPropertiesKHR"
                << vkResultName(res) << int(res)
                << "plane" << i
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedNativeTexture(&imported);
            return false;
        }

        VkImagePlaneMemoryRequirementsInfo planeReqInfo = {};
        VkImageMemoryRequirementsInfo2 memoryReqInfo = {};
        memoryReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memoryReqInfo.image = imported.image;

        if (disjoint) {
            planeReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
            planeReqInfo.planeAspect = vulkanMemoryPlaneAspect(i);
            memoryReqInfo.pNext = &planeReqInfo;
        }

        VkMemoryRequirements2 memoryReq = {};
        memoryReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        vkGetImageMemoryRequirements2(device, &memoryReqInfo, &memoryReq);

        const int memoryType = findVulkanMemoryType(
            vkGetPhysicalDeviceMemoryProperties,
            physicalDevice,
            0,
            memoryReq.memoryRequirements.memoryTypeBits & fdProps.memoryTypeBits);
        if (memoryType < 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI client texture import failed: no compatible memory type"
                << "plane" << i
                << "requirements" << Qt::hex << memoryReq.memoryRequirements.memoryTypeBits
                << "fd" << fdProps.memoryTypeBits << Qt::dec
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedNativeTexture(&imported);
            return false;
        }

        const int dupFd = fcntl(attribs->fd[i], F_DUPFD_CLOEXEC, 0);
        if (dupFd < 0) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI client texture import failed: failed to duplicate dmabuf fd"
                << "plane" << i
                << "errno" << errno
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedNativeTexture(&imported);
            return false;
        }

        VkImportMemoryFdInfoKHR importFdInfo = {};
        importFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        importFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        importFdInfo.fd = dupFd;

        VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
        dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedInfo.image = imported.image;
        importFdInfo.pNext = &dedicatedInfo;

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = &importFdInfo;
        allocInfo.allocationSize = memoryReq.memoryRequirements.size;
        allocInfo.memoryTypeIndex = uint32_t(memoryType);

        res = vkAllocateMemory(device, &allocInfo, nullptr, &imported.memories[i]);
        if (res != VK_SUCCESS) {
            close(dupFd);
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI client texture import failed: vkAllocateMemory"
                << vkResultName(res) << int(res)
                << "plane" << i
                << "allocationSize" << qulonglong(allocInfo.allocationSize)
                << "memoryType" << memoryType
                << "format" << drmFormatToName(attribs->format)
                << "modifier" << drmModifierToName(attribs->modifier);
            destroyVulkanImportedNativeTexture(&imported);
            return false;
        }
        ++imported.memoryCount;

        bindInfos[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
        bindInfos[i].image = imported.image;
        bindInfos[i].memory = imported.memories[i];
        bindInfos[i].memoryOffset = 0;

        if (disjoint) {
            planeInfos[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
            planeInfos[i].planeAspect = vulkanMemoryPlaneAspect(i);
            bindInfos[i].pNext = &planeInfos[i];
        }
    }

    res = vkBindImageMemory2(device, memoryCount, bindInfos);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture import failed: vkBindImageMemory2"
            << vkResultName(res) << int(res)
            << "format" << drmFormatToName(attribs->format)
            << "modifier" << drmModifierToName(attribs->modifier)
            << "planes" << attribs->n_planes
            << "memoryCount" << memoryCount
            << "disjoint" << disjoint;
        destroyVulkanImportedNativeTexture(&imported);
        return false;
    }

    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI client texture import succeeded"
        << "image" << Qt::hex << vulkanHandleToInteger(imported.image) << Qt::dec
        << "size" << imported.size
        << "format" << drmFormatToName(imported.drmFormat)
        << "modifier" << drmModifierToName(imported.drmModifier)
        << "imageVkFormat" << vkFormat
        << "viewVkFormat" << imported.format
        << "planes" << attribs->n_planes
        << "memoryCount" << imported.memoryCount
        << "disjoint" << disjoint
        << "mutableSrgbView" << useMutableSrgbView;

    *out = imported;
    return true;
}

static bool acquireVulkanNativeTextureForSampling(QRhi *rhi,
                                                  VulkanImportedNativeTexture *texture)
{
    if (!rhi || !texture || !texture->isValid())
        return false;

    const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!handles || !handles->dev || !handles->gfxQueue || !handles->inst) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture acquire failed: QRhi Vulkan native handles unavailable";
        return false;
    }

    if (handles->dev != texture->device) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture acquire failed: QRhi device differs from imported texture"
            << "rhi" << Qt::hex << vulkanHandleToInteger(handles->dev)
            << "texture" << vulkanHandleToInteger(texture->device) << Qt::dec;
        return false;
    }

    VkDevice device = handles->dev;
    VkQueue queue = handles->gfxQueue;
    texture->queue = queue;
    texture->queueFamilyIndex = handles->gfxQueueFamilyIdx;

    const VkImageLayout oldLayout = texture->layout;
    if (!submitVulkanImageBarrier(rhi,
                                  device,
                                  texture->image,
                                  nullptr,
                                  "Vulkan RHI client texture",
                                  "acquire",
                                  oldLayout,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_QUEUE_FAMILY_FOREIGN_EXT,
                                  handles->gfxQueueFamilyIdx,
                                  0,
                                  VK_ACCESS_SHADER_READ_BIT,
                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                  false)) {
        return false;
    }

    texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI client texture acquired for Qt sampling"
        << "image" << Qt::hex << vulkanHandleToInteger(texture->image) << Qt::dec
        << "oldLayout" << vkImageLayoutName(oldLayout)
        << "newLayout" << vkImageLayoutName(texture->layout)
        << "format" << drmFormatToName(texture->drmFormat)
        << "modifier" << drmModifierToName(texture->drmModifier)
        << "size" << texture->size
        << "srcQueue" << VK_QUEUE_FAMILY_FOREIGN_EXT
        << "dstQueue" << handles->gfxQueueFamilyIdx;

    return true;
}

// Import a dmabuf as a GL texture via EGL, so Qt RHI (GL) can render into it
// even when the wlroots renderer is Vulkan. This mirrors wlroots'
// wlr_egl_create_image_from_dmabuf (render/egl.c) but uses the Qt RHI GL
// context's EGL display instead of wlroots' gles2 EGL. dmabuf is API-agnostic:
// any EGL context can import it regardless of which renderer created it.
// On success, sets *outImage and *outTex; caller owns both (destroy via
// eglDestroyImageKHR / glDeleteTextures).
static bool eglImportDmabufToGLTexture(EGLDisplay display,
                                       const wlr_dmabuf_attributes *attribs,
                                       EGLImage *outImage, GLuint *outTex)
{
    flushPendingNativeTextureCleanups();

    auto eglCreateImageKHR = resolveEglCreateImageKHR();
    auto glEGLImageTargetTexture2DOES = resolveGlEGLImageTargetTexture2DOES();
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        qCWarning(lcWlRenderHelper) << "EGL dmabuf import: eglCreateImageKHR or glEGLImageTargetTexture2DOES not available";
        return false;
    }

    // Build EGL attribute list, mirroring wlroots egl.c:733-830.
    EGLint eglAttribs[50];
    unsigned int atti = 0;
    eglAttribs[atti++] = EGL_WIDTH;
    eglAttribs[atti++] = attribs->width;
    eglAttribs[atti++] = EGL_HEIGHT;
    eglAttribs[atti++] = attribs->height;
    eglAttribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    eglAttribs[atti++] = attribs->format;

    static const struct {
        EGLint fd, offset, pitch, mod_lo, mod_hi;
    } plane_attrs[WLR_DMABUF_MAX_PLANES] = {
        { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT,
          EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
          EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT },
        { EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
          EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
          EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT },
        { EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT,
          EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
          EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT },
        { EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT,
          EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
          EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT },
    };

    for (int i = 0; i < attribs->n_planes; i++) {
        eglAttribs[atti++] = plane_attrs[i].fd;
        eglAttribs[atti++] = attribs->fd[i];
        eglAttribs[atti++] = plane_attrs[i].offset;
        eglAttribs[atti++] = attribs->offset[i];
        eglAttribs[atti++] = plane_attrs[i].pitch;
        eglAttribs[atti++] = attribs->stride[i];
        if (attribs->modifier != DRM_FORMAT_MOD_INVALID) {
            eglAttribs[atti++] = plane_attrs[i].mod_lo;
            eglAttribs[atti++] = EGLint(attribs->modifier & 0xFFFFFFFF);
            eglAttribs[atti++] = plane_attrs[i].mod_hi;
            eglAttribs[atti++] = EGLint(attribs->modifier >> 32);
        }
    }
    eglAttribs[atti++] = EGL_IMAGE_PRESERVED_KHR;
    eglAttribs[atti++] = EGL_TRUE;
    eglAttribs[atti++] = EGL_NONE;

    EGLImage image = eglCreateImageKHR(display, EGL_NO_CONTEXT,
                                        EGL_LINUX_DMA_BUF_EXT, NULL, eglAttribs);
    if (image == EGL_NO_IMAGE) {
        qCWarning(lcWlRenderHelper) << "EGL dmabuf import: eglCreateImageKHR failed, EGL error=" << eglGetError();
        return false;
    }

    GLuint tex = 0;
    clearGlErrors();
    glGenTextures(1, &tex);
    if (!tex) {
        if (auto destroyImage = resolveEglDestroyImageKHR())
            destroyImage(display, image);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!textureUploadSucceeded()) {
        glDeleteTextures(1, &tex);
        if (auto destroyImage = resolveEglDestroyImageKHR())
            destroyImage(display, image);
        return false;
    }

    *outImage = image;
    *outTex = tex;
    return true;
}
#endif

void WRenderHelper::releaseNativeTexture(NativeTextureCleanup *cleanup)
{
    if (!cleanup || cleanup->type == NativeTextureCleanup::Type::None)
        return;

#ifdef ENABLE_VULKAN_RENDER
    if (cleanup->type == NativeTextureCleanup::Type::VulkanTexture) {
        releaseNativeTextureNow(cleanup);
        return;
    }

    if (cleanup->type == NativeTextureCleanup::Type::OpenGLTexture) {
        if (QOpenGLContext::currentContext())
            flushPendingNativeTextureCleanups();
        if (!releaseNativeTextureNow(cleanup))
            queueNativeTextureCleanup(cleanup);
        return;
    }
#endif

    *cleanup = {};
}

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
    std::unique_ptr<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    rt->setFlags(flags);
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
        if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
            qCDebug(lcWlRenderHelper)
                << "Resolving native texture render target into QRhiTexture"
                << "nativeObject" << Qt::hex << rtd->u.nativeTexture.object << Qt::dec
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
                << "layoutOrState" << rtd->u.nativeTexture.layout
#else
                << "layoutOrState" << rtd->u.nativeTexture.layoutOrState
#endif
                << "pixelSize" << rtd->pixelSize
                << "sampleCount" << rtd->sampleCount
                << "rhiFormat" << format
                << "formatFlags" << int(flags);
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layout }))
#else
        if (!texture->createFrom({ rtd->u.nativeTexture.object, rtd->u.nativeTexture.layoutOrState }))
#endif
        {
            qCWarning(lcWlRenderHelper) << "Failed to wrap native texture (VkImage/GL texture) into QRhiTexture for render target";
            return false;
        }
        if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
            qCDebug(lcWlRenderHelper)
                << "Resolved native texture render target into QRhiTexture"
                << "texturePtr" << quintptr(texture.get())
                << "pixelSize" << texture->pixelSize()
                << "sampleCount" << rtd->sampleCount;
        }
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
    WRenderHelperPrivate(WRenderHelper *qq, qw_renderer *renderer)
        : WObjectPrivate(qq)
        , renderer(renderer)
    {}
    ~WRenderHelperPrivate() {
        resetRenderBuffer();
    }

    void resetRenderBuffer();
    void onBufferDestroy();
    static bool ensureRhiRenderTarget(QQuickRenderControl *rc, BufferData *data);

    W_DECLARE_PUBLIC(WRenderHelper)
    qw_renderer *renderer;
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

void WRenderHelperPrivate::onBufferDestroy()
{
    qw_buffer *buffer = qobject_cast<qw_buffer*>(q_func()->sender());

    for (int i = 0; i < buffers.count(); ++i) {
        auto data = buffers[i];
        if (data->buffer == buffer) {
            if (lastBuffer == data)
                lastBuffer = nullptr;
            buffers.removeAt(i);
#ifdef ENABLE_VULKAN_RENDER
            if (renderer && renderer->is_vk())
                delete data;
#endif
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

#ifdef ENABLE_VULKAN_RENDER
    if (data->vulkanRenderTarget.isValid() && rhi && rhi->backend() == QRhi::Vulkan) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QRhiTexture *sharedTexture = data->windowRenderTarget.res.texture;
        QRhiRenderTarget *clearRenderTarget = data->windowRenderTarget.rt.renderTarget;
#else
        QRhiTexture *sharedTexture = data->windowRenderTarget.texture;
        QRhiRenderTarget *clearRenderTarget = data->windowRenderTarget.renderTarget;
#endif
        if (!sharedTexture || !clearRenderTarget) {
            qCWarning(lcWlRenderHelper)
                << "Failed to build Vulkan output preserve render target:"
                << "clear target or shared texture is missing"
                << "bufferPtr" << quintptr(data->buffer)
                << "image" << Qt::hex
                << vulkanHandleToInteger(data->vulkanRenderTarget.image)
                << Qt::dec
                << "size" << data->vulkanRenderTarget.size;
            return false;
        }

        QRhiColorAttachment preserveAttachment(sharedTexture);
        if (!createRhiRenderTarget(preserveAttachment,
                                   data->vulkanRenderTarget.size,
                                   1,
                                   rhi,
                                   data->preserveWindowRenderTarget,
                                   QRhiTextureRenderTarget::PreserveColorContents)) {
            qCWarning(lcWlRenderHelper)
                << "Failed to build Vulkan output preserve render target"
                << "bufferPtr" << quintptr(data->buffer)
                << "image" << Qt::hex
                << vulkanHandleToInteger(data->vulkanRenderTarget.image)
                << Qt::dec
                << "size" << data->vulkanRenderTarget.size;
            return false;
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        data->preserveRenderTarget =
            QQuickRenderTarget::fromRhiRenderTarget(data->preserveWindowRenderTarget.rt.renderTarget);
#else
        data->preserveRenderTarget =
            QQuickRenderTarget::fromRhiRenderTarget(data->preserveWindowRenderTarget.renderTarget);
#endif
        data->preserveRenderTarget.setDevicePixelRatio(tmp.devicePixelRatio());
        data->preserveRenderTarget.setMirrorVertically(tmp.mirrorVertically());

        if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            auto *preserveRt = data->preserveWindowRenderTarget.rt.renderTarget;
#else
            auto *preserveRt = data->preserveWindowRenderTarget.renderTarget;
#endif
            qCDebug(lcWlRenderHelper)
                << "Created Vulkan output clear/load render target pair"
                << "bufferPtr" << quintptr(data->buffer)
                << "clearRenderTargetPtr" << quintptr(clearRenderTarget)
                << "loadRenderTargetPtr" << quintptr(preserveRt)
                << "sharedTexturePtr" << quintptr(sharedTexture)
                << "sharedTextureSize" << sharedTexture->pixelSize()
                << "image" << Qt::hex
                << vulkanHandleToInteger(data->vulkanRenderTarget.image)
                << Qt::dec
                << "format" << drmFormatToName(data->vulkanRenderTarget.drmFormat)
                << "modifier" << drmModifierToName(data->vulkanRenderTarget.drmModifier);
        }
    }
#endif

    if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
        const auto *sourceRt = QQuickRenderTargetPrivate::get(&tmp);
        const auto *resolvedRt = QQuickRenderTargetPrivate::get(&data->renderTarget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        auto *rhiRenderTarget = data->windowRenderTarget.rt.renderTarget;
        auto *rhiTexture = data->windowRenderTarget.res.texture;
        auto *preserveRhiRenderTarget = data->preserveWindowRenderTarget.rt.renderTarget;
#else
        auto *rhiRenderTarget = data->windowRenderTarget.renderTarget;
        auto *rhiTexture = data->windowRenderTarget.texture;
        auto *preserveRhiRenderTarget = data->preserveWindowRenderTarget.renderTarget;
#endif
        qCDebug(lcWlRenderHelper)
            << "Resolved QQuickRenderTarget for Qt RHI rendering"
            << "bufferPtr" << quintptr(data->buffer)
            << "sourceType" << quickRenderTargetTypeName(sourceRt->type)
            << "resolvedType" << quickRenderTargetTypeName(resolvedRt->type)
            << "pixelSize" << sourceRt->pixelSize
            << "resolvedPixelSize" << resolvedRt->pixelSize
            << "sampleCount" << sourceRt->sampleCount
            << "mirrorVertically" << data->renderTarget.mirrorVertically()
            << "devicePixelRatio" << data->renderTarget.devicePixelRatio()
            << "rhiRenderTargetPtr" << quintptr(rhiRenderTarget)
            << "preserveRhiRenderTargetPtr" << quintptr(preserveRhiRenderTarget)
            << "rhiTexturePtr" << quintptr(rhiTexture)
            << "rhiTextureSize" << (rhiTexture ? rhiTexture->pixelSize() : QSize());
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
        Q_ASSERT(renderer->is_gles2());
        auto egl = wlr_gles2_renderer_get_egl(renderer->handle());

        return qw_buffer::create(new GLTextureBuffer(egl, texture), size.width(), size.height());
    }
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        Q_ASSERT(renderer->is_vk());
        auto instance = renderer->get_instance();
        auto device = renderer->get_device();

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
#ifdef ENABLE_VULKAN_RENDER
            if (data->vulkanRenderTarget.isValid()) {
                if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
                    const auto *rtData = QQuickRenderTargetPrivate::get(&data->renderTarget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
                    auto *rhiRenderTarget = data->windowRenderTarget.rt.renderTarget;
                    auto *preserveRhiRenderTarget = data->preserveWindowRenderTarget.rt.renderTarget;
                    auto *rhiTexture = data->windowRenderTarget.res.texture;
#else
                    auto *rhiRenderTarget = data->windowRenderTarget.renderTarget;
                    auto *preserveRhiRenderTarget = data->preserveWindowRenderTarget.renderTarget;
                    auto *rhiTexture = data->windowRenderTarget.texture;
#endif
                    qCDebug(lcWlRenderHelper)
                        << "Reusing Qt render target for Vulkan output buffer"
                        << "bufferPtr" << quintptr(buffer)
                        << "renderTargetType" << quickRenderTargetTypeName(rtData->type)
                        << "renderTargetPixelSize" << rtData->pixelSize
                        << "rhiRenderTargetPtr" << quintptr(rhiRenderTarget)
                        << "preserveRhiRenderTargetPtr" << quintptr(preserveRhiRenderTarget)
                        << "rhiTexturePtr" << quintptr(rhiTexture)
                        << "rhiTextureSize" << (rhiTexture ? rhiTexture->pixelSize() : QSize())
                        << "image" << Qt::hex
                        << vulkanHandleToInteger(data->vulkanRenderTarget.image)
                        << Qt::dec
                        << "layout" << vkImageLayoutName(data->vulkanRenderTarget.layout)
                        << "ownerIsForeign" << data->vulkanRenderTarget.ownerIsForeign
                        << "scanoutReleaseReady" << data->vulkanRenderTarget.scanoutReleaseReady
                        << "format" << drmFormatToName(data->vulkanRenderTarget.drmFormat)
                        << "modifier" << drmModifierToName(data->vulkanRenderTarget.drmModifier);
                }
                qCDebug(lcWlRenderHelper)
                    << "Reusing Vulkan RHI output render target import"
                    << "image" << Qt::hex
                    << vulkanHandleToInteger(data->vulkanRenderTarget.image)
                    << Qt::dec
                    << "size" << data->vulkanRenderTarget.size
                    << "format" << drmFormatToName(data->vulkanRenderTarget.drmFormat)
                    << "modifier" << drmModifierToName(data->vulkanRenderTarget.drmModifier);
            }
#endif
            return data->renderTarget;
        }
    }

    std::unique_ptr<BufferData> bufferData(new BufferData);
    bufferData->buffer = buffer;

    QQuickRenderTarget rt;

    if (d->renderer->is_pixman()) {
        auto texture = qw_texture::from_buffer(*d->renderer, *buffer);
        pixman_image_t *image = texture->get_image();
        void *data = pixman_image_get_data(image);
        if (bufferData->paintDevice.constBits() != data)
            bufferData->paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!bufferData->paintDevice.isNull());
        rt = QQuickRenderTarget::fromPaintDevice(&bufferData->paintDevice);
        delete texture;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (d->renderer->is_vk()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
        auto rhi = QQuickRenderControlPrivate::get(rc)->rhi;
#else
        auto rhi = rc->rhi();
#endif
        if (rhi && rhi->backend() == QRhi::Vulkan) {
            const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
            if (!handles || !handles->inst || handles->inst->vkInstance() == VK_NULL_HANDLE
                || handles->physDev == VK_NULL_HANDLE || handles->dev == VK_NULL_HANDLE) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: QRhi Vulkan native handles unavailable";
                return {};
            }

            if (!d->renderer || !d->renderer->handle() || !d->renderer->is_vk()) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: wlroots renderer is not Vulkan";
                return {};
            }

            const VkDevice wlrDevice = d->renderer->get_device();
            if (wlrDevice == VK_NULL_HANDLE || wlrDevice != handles->dev) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: Qt RHI and wlroots use different VkDevice"
                    << "qt" << Qt::hex << vulkanHandleToInteger(handles->dev)
                    << "wlroots" << vulkanHandleToInteger(wlrDevice) << Qt::dec;
                return {};
            }

            wlr_dmabuf_attributes dmabuf = {};
            if (!buffer->get_dmabuf(&dmabuf)) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: output buffer has no dmabuf"
                    << buffer << "size" << d->size;
                return {};
            }

            if (dmabuf.width != d->size.width() || dmabuf.height != d->size.height()) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import rejected: dmabuf size differs from render target size"
                    << "dmabuf" << QSize(dmabuf.width, dmabuf.height)
                    << "target" << d->size
                    << "format" << drmFormatToName(dmabuf.format)
                    << "modifier" << drmModifierToName(dmabuf.modifier);
                return {};
            }

            constexpr VkImageUsageFlags outputRenderTargetUsage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            if (!importDmabufAsVulkanRenderTarget(handles->inst->vkInstance(),
                                                  handles->physDev,
                                                  handles->dev,
                                                  &dmabuf,
                                                  outputRenderTargetUsage,
                                                  true,
                                                  &bufferData->vulkanRenderTarget)) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import failed: cannot create render target for output buffer"
                    << buffer
                    << "size" << d->size
                    << "format" << drmFormatToName(dmabuf.format)
                    << "modifier" << drmModifierToName(dmabuf.modifier)
                    << "planes" << dmabuf.n_planes
                    << "usage" << Qt::hex << outputRenderTargetUsage << Qt::dec;
                return {};
            }

            rt = QQuickRenderTarget::fromVulkanImage(bufferData->vulkanRenderTarget.image,
                                                     bufferData->vulkanRenderTarget.layout,
                                                     bufferData->vulkanRenderTarget.format,
                                                     bufferData->vulkanRenderTarget.size,
                                                     1);
            if (rt.isNull()) {
                qCWarning(lcWlRenderHelper)
                    << "Vulkan RHI output import failed: QQuickRenderTarget::fromVulkanImage returned null"
                    << "image" << Qt::hex
                    << vulkanHandleToInteger(bufferData->vulkanRenderTarget.image)
                    << Qt::dec
                    << "size" << bufferData->vulkanRenderTarget.size
                    << "format" << bufferData->vulkanRenderTarget.format;
                return {};
            }
            if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
                const auto *rtData = QQuickRenderTargetPrivate::get(&rt);
                qCDebug(lcWlRenderHelper)
                    << "Created QQuickRenderTarget from imported Vulkan output image"
                    << "bufferPtr" << quintptr(buffer)
                    << "renderTargetType" << quickRenderTargetTypeName(rtData->type)
                    << "image" << Qt::hex
                    << vulkanHandleToInteger(bufferData->vulkanRenderTarget.image)
                    << Qt::dec
                    << "layout" << vkImageLayoutName(bufferData->vulkanRenderTarget.layout)
                    << "pixelSize" << rtData->pixelSize
                    << "sampleCount" << rtData->sampleCount
                    << "vkFormat" << bufferData->vulkanRenderTarget.format
                    << "format" << drmFormatToName(bufferData->vulkanRenderTarget.drmFormat)
                    << "modifier" << drmModifierToName(bufferData->vulkanRenderTarget.drmModifier)
                    << "usage" << Qt::hex << outputRenderTargetUsage << Qt::dec
                    << "ownerIsForeign" << bufferData->vulkanRenderTarget.ownerIsForeign;
            }
        } else {
            // Vulkan wlroots renderer with GL Qt RHI: import the output buffer's
            // dmabuf as a GL texture via EGL (EGL_EXT_image_dma_buf_import), so Qt
            // RHI (GL) can render into it. This remains as an explicit fallback
            // when Qt Quick is forced to OpenGL while wlroots uses Vulkan.
            EGLDisplay eglDisplay = eglGetCurrentDisplay();
            if (eglDisplay == EGL_NO_DISPLAY) {
                qCWarning(lcWlRenderHelper) << "Vulkan+GL: no current EGL display (GL context not current?)";
                return {};
            }

            wlr_dmabuf_attributes dmabuf;
            if (!buffer->get_dmabuf(&dmabuf)) {
                qCWarning(lcWlRenderHelper) << "Vulkan+GL: output buffer has no dmabuf";
                return {};
            }

            EGLImage eglImage = EGL_NO_IMAGE;
            GLuint glTex = 0;
            if (!eglImportDmabufToGLTexture(eglDisplay, &dmabuf, &eglImage, &glTex)) {
                qCWarning(lcWlRenderHelper) << "Vulkan+GL: EGL dmabuf import failed for output buffer";
                return {};
            }

            bufferData->eglImage = eglImage;
            bufferData->glTexture = glTex;
            bufferData->eglDisplay = eglDisplay;

            rt = QQuickRenderTarget::fromOpenGLTexture(glTex, d->size);
            rt.setMirrorVertically(true);
        }
    }
#endif
    else if (d->renderer->is_gles2()) {
        auto texture = qw_texture::from_buffer(*d->renderer, *buffer);
        wlr_gles2_texture_attribs attribs;
        texture->get_attribs(&attribs);

        rt = QQuickRenderTarget::fromOpenGLTexture(attribs.tex, d->size);
        rt.setMirrorVertically(true);
        delete texture;
    }
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

    if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
        const auto *rtData = QQuickRenderTargetPrivate::get(&d->lastBuffer->renderTarget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        auto *rhiRenderTarget = d->lastBuffer->windowRenderTarget.rt.renderTarget;
        auto *preserveRhiRenderTarget = d->lastBuffer->preserveWindowRenderTarget.rt.renderTarget;
        auto *rhiTexture = d->lastBuffer->windowRenderTarget.res.texture;
#else
        auto *rhiRenderTarget = d->lastBuffer->windowRenderTarget.renderTarget;
        auto *preserveRhiRenderTarget = d->lastBuffer->preserveWindowRenderTarget.renderTarget;
        auto *rhiTexture = d->lastBuffer->windowRenderTarget.texture;
#endif
        qCDebug(lcWlRenderHelper)
            << "Acquired render target for output buffer"
            << "bufferPtr" << quintptr(buffer)
            << "renderTargetType" << quickRenderTargetTypeName(rtData->type)
            << "renderTargetPixelSize" << rtData->pixelSize
            << "mirrorVertically" << d->lastBuffer->renderTarget.mirrorVertically()
            << "devicePixelRatio" << d->lastBuffer->renderTarget.devicePixelRatio()
            << "rhiRenderTargetPtr" << quintptr(rhiRenderTarget)
            << "preserveRhiRenderTargetPtr" << quintptr(preserveRhiRenderTarget)
            << "rhiTexturePtr" << quintptr(rhiTexture)
            << "rhiTextureSize" << (rhiTexture ? rhiTexture->pixelSize() : QSize())
#ifdef ENABLE_VULKAN_RENDER
            << "hasVulkanImport" << d->lastBuffer->vulkanRenderTarget.isValid()
            << "vulkanImage" << Qt::hex
            << vulkanHandleToInteger(d->lastBuffer->vulkanRenderTarget.image)
            << Qt::dec
            << "vulkanLayout" << vkImageLayoutName(d->lastBuffer->vulkanRenderTarget.layout)
            << "ownerIsForeign" << d->lastBuffer->vulkanRenderTarget.ownerIsForeign
            << "scanoutReleaseReady" << d->lastBuffer->vulkanRenderTarget.scanoutReleaseReady
#endif
            ;
    }

    return d->buffers.last()->renderTarget;
}

QQuickRenderTarget WRenderHelper::renderTargetForBuffer(qw_buffer *buffer,
                                                        bool preserveColorContents) const
{
    W_DC(WRenderHelper);
    if (!buffer)
        return {};

    for (auto bufferData : std::as_const(d->buffers)) {
        if (bufferData->buffer != buffer)
            continue;

#ifdef ENABLE_VULKAN_RENDER
        if (preserveColorContents && bufferData->vulkanRenderTarget.isValid()) {
            if (!bufferData->preserveRenderTarget.isNull())
                return bufferData->preserveRenderTarget;

            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI output preserve render target unavailable"
                << "bufferPtr" << quintptr(buffer)
                << "image" << Qt::hex
                << vulkanHandleToInteger(bufferData->vulkanRenderTarget.image)
                << Qt::dec
                << "size" << bufferData->vulkanRenderTarget.size;
            return {};
        }
#else
        Q_UNUSED(preserveColorContents);
#endif
        return bufferData->renderTarget;
    }

    return {};
}

bool WRenderHelper::prepareVulkanRenderTargetForQt(QRhi *rhi, QRhiTexture *texture,
                                                   qw_buffer *buffer)
{
#ifdef ENABLE_VULKAN_RENDER
    W_D(WRenderHelper);
    if (!rhi || !texture || !buffer || !d->renderer || !d->renderer->handle()
        || !d->renderer->is_vk() || rhi->backend() != QRhi::Vulkan) {
        return true;
    }

    BufferData *data = nullptr;
    for (auto bufferData : std::as_const(d->buffers)) {
        if (bufferData->buffer == buffer) {
            data = bufferData;
            break;
        }
    }

    if (!data || !data->vulkanRenderTarget.isValid())
        return true;

    VulkanImportedRenderTarget &target = data->vulkanRenderTarget;
    const auto nativeTexture = texture->nativeTexture();
    const VkImage textureImage = reinterpret_cast<VkImage>(static_cast<quintptr>(nativeTexture.object));
    if (textureImage != target.image) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output acquire rejected: QRhi texture image differs from imported output image"
            << "texture" << Qt::hex << vulkanHandleToInteger(textureImage)
            << "target" << vulkanHandleToInteger(target.image) << Qt::dec
            << "size" << target.size;
        return false;
    }

    const bool needsOwnershipAcquire = target.ownerIsForeign;
    const bool needsLayoutTransition = target.layout != VK_IMAGE_LAYOUT_GENERAL;
    if (!needsOwnershipAcquire && !needsLayoutTransition) {
        texture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);
        if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
            qCDebug(lcWlRenderHelper)
                << "Vulkan RHI output acquire kept Qt ownership"
                << "bufferPtr" << quintptr(buffer)
                << "texturePtr" << quintptr(texture)
                << "image" << Qt::hex << vulkanHandleToInteger(target.image) << Qt::dec
                << "layout" << vkImageLayoutName(target.layout)
                << "ownerIsForeign" << target.ownerIsForeign
                << "scanoutReleaseReady" << target.scanoutReleaseReady
                << "nativeLayout" << vkImageLayoutName(static_cast<VkImageLayout>(nativeTexture.layout))
                << "size" << target.size
                << "format" << drmFormatToName(target.drmFormat)
                << "modifier" << drmModifierToName(target.drmModifier);
        }
        return true;
    }

    if (needsOwnershipAcquire
        && !waitDmabufImplicitFence(buffer, DMA_BUF_SYNC_WRITE,
                                    "Vulkan RHI output", "acquire")) {
        return false;
    }

    const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!handles) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output acquire failed: QRhi Vulkan native handles unavailable";
        return false;
    }

    const uint32_t queueFamily = static_cast<uint32_t>(handles->gfxQueueFamilyIdx);
    const uint32_t srcQueue = needsOwnershipAcquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : VK_QUEUE_FAMILY_IGNORED;
    const uint32_t dstQueue = needsOwnershipAcquire ? queueFamily : VK_QUEUE_FAMILY_IGNORED;
    const VkImageLayout oldLayout = target.layout;

    if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI output acquire transition requested"
            << "bufferPtr" << quintptr(buffer)
            << "texturePtr" << quintptr(texture)
            << "image" << Qt::hex << vulkanHandleToInteger(target.image) << Qt::dec
            << "oldLayout" << vkImageLayoutName(oldLayout)
            << "newLayout" << vkImageLayoutName(VK_IMAGE_LAYOUT_GENERAL)
            << "needsOwnershipAcquire" << needsOwnershipAcquire
            << "needsLayoutTransition" << needsLayoutTransition
            << "srcQueue" << srcQueue
            << "dstQueue" << dstQueue
            << "nativeLayout" << vkImageLayoutName(static_cast<VkImageLayout>(nativeTexture.layout))
            << "size" << target.size
            << "format" << drmFormatToName(target.drmFormat)
            << "modifier" << drmModifierToName(target.drmModifier);
    }

    if (!submitVulkanOutputTargetBarrier(rhi,
                                         &target,
                                         buffer,
                                         "acquire",
                                         oldLayout,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         srcQueue,
                                         dstQueue,
                                         0,
                                         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                             | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         false)) {
        return false;
    }

    target.layout = VK_IMAGE_LAYOUT_GENERAL;
    target.ownerIsForeign = false;
    target.scanoutReleaseReady = false;
    texture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);

    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI output acquired for Qt rendering"
        << "image" << Qt::hex << vulkanHandleToInteger(target.image) << Qt::dec
        << "oldLayout" << vkImageLayoutName(oldLayout)
        << "newLayout" << vkImageLayoutName(target.layout)
        << "format" << drmFormatToName(target.drmFormat)
        << "modifier" << drmModifierToName(target.drmModifier)
        << "size" << target.size;

    return true;
#else
    Q_UNUSED(rhi);
    Q_UNUSED(texture);
    Q_UNUSED(buffer);
    return true;
#endif
}

bool WRenderHelper::releaseVulkanRenderTargetToScanout(QRhi *rhi, QRhiTexture *texture,
                                                       qw_buffer *buffer)
{
#ifdef ENABLE_VULKAN_RENDER
    W_D(WRenderHelper);
    if (!rhi || !texture || !buffer || !d->renderer || !d->renderer->handle()
        || !d->renderer->is_vk() || rhi->backend() != QRhi::Vulkan) {
        return true;
    }

    BufferData *data = nullptr;
    for (auto bufferData : std::as_const(d->buffers)) {
        if (bufferData->buffer == buffer) {
            data = bufferData;
            break;
        }
    }

    if (!data || !data->vulkanRenderTarget.isValid())
        return true;

    VulkanImportedRenderTarget &target = data->vulkanRenderTarget;
    const auto nativeTexture = texture->nativeTexture();
    const VkImage textureImage = reinterpret_cast<VkImage>(static_cast<quintptr>(nativeTexture.object));
    if (textureImage != target.image) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output release rejected: QRhi texture image differs from imported output image"
            << "texture" << Qt::hex << vulkanHandleToInteger(textureImage)
            << "target" << vulkanHandleToInteger(target.image) << Qt::dec
            << "size" << target.size;
        return false;
    }

    if (target.ownerIsForeign) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI output release skipped: target already owned by foreign queue"
            << "image" << Qt::hex << vulkanHandleToInteger(target.image) << Qt::dec
            << "layout" << vkImageLayoutName(target.layout);
        return target.scanoutReleaseReady;
    }

    const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!handles) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI output release failed: QRhi Vulkan native handles unavailable";
        return false;
    }

    const uint32_t queueFamily = static_cast<uint32_t>(handles->gfxQueueFamilyIdx);
    const VkImageLayout oldLayout = static_cast<VkImageLayout>(nativeTexture.layout);

    if (Q_UNLIKELY(lcWlRenderHelper().isDebugEnabled())) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI output release transition requested"
            << "bufferPtr" << quintptr(buffer)
            << "texturePtr" << quintptr(texture)
            << "image" << Qt::hex << vulkanHandleToInteger(target.image) << Qt::dec
            << "oldLayout" << vkImageLayoutName(oldLayout)
            << "trackedLayout" << vkImageLayoutName(target.layout)
            << "newLayout" << vkImageLayoutName(VK_IMAGE_LAYOUT_GENERAL)
            << "queueFamily" << queueFamily
            << "ownerIsForeign" << target.ownerIsForeign
            << "scanoutReleaseReady" << target.scanoutReleaseReady
            << "size" << target.size
            << "format" << drmFormatToName(target.drmFormat)
            << "modifier" << drmModifierToName(target.drmModifier);
    }

    if (!submitVulkanOutputTargetBarrier(rhi,
                                         &target,
                                         buffer,
                                         "release",
                                         oldLayout,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         queueFamily,
                                         VK_QUEUE_FAMILY_FOREIGN_EXT,
                                         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                             | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         0,
                                         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                         true)) {
        target.scanoutReleaseReady = false;
        return false;
    }

    target.layout = VK_IMAGE_LAYOUT_GENERAL;
    target.ownerIsForeign = true;
    target.scanoutReleaseReady = true;
    texture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);

    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI output released for scanout"
        << "image" << Qt::hex << vulkanHandleToInteger(target.image) << Qt::dec
        << "oldLayout" << vkImageLayoutName(oldLayout)
        << "newLayout" << vkImageLayoutName(target.layout)
        << "format" << drmFormatToName(target.drmFormat)
        << "modifier" << drmModifierToName(target.drmModifier)
        << "size" << target.size;

    return true;
#else
    Q_UNUSED(rhi);
    Q_UNUSED(texture);
    Q_UNUSED(buffer);
    return true;
#endif
}

void WRenderHelper::transitionVkImageToGeneral(QRhi *rhi, QRhiTexture *texture,
                                               qw_buffer *buffer)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!rhi || !texture || !buffer)
        return;

    // Obtain the wlroots-adopted VkDevice/VkQueue from Qt RHI. QRhiVulkanNativeHandles
    // (qrhi_platform.h) exposes dev, gfxQueue, gfxQueueFamilyIdx and inst, all of
    // which belong to the wlroots-adopted Vulkan device.
    const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!handles || !handles->dev || !handles->gfxQueue || !handles->inst) {
        qCWarning(lcWlRenderHelper) << "Vulkan: QRhi native handles unavailable, cannot transition render image layout";
        return;
    }

    VkDevice device = handles->dev;
    VkQueue queue = handles->gfxQueue;
    VkImage image = reinterpret_cast<VkImage>(static_cast<quintptr>(texture->nativeTexture().object));

    // Device-level functions must be resolved via vkGetDeviceProcAddr, which
    // returns device-relative entry points (dispatch table or layer chain),
    // per the Vulkan loader rules (trampoline.c vkGetDeviceProcAddr). All
    // functions used below are device-level commands.
    auto vkGetDeviceProcAddr = resolveVkGetDeviceProcAddr();
    if (!vkGetDeviceProcAddr) {
        qCWarning(lcWlRenderHelper) << "Vulkan: vkGetDeviceProcAddr unavailable, cannot transition render image layout";
        return;
    }
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers =
        reinterpret_cast<PFN_vkAllocateCommandBuffers>(vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer =
        reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier =
        reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier"));
    PFN_vkEndCommandBuffer vkEndCommandBuffer =
        reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
    PFN_vkQueueSubmit vkQueueSubmit =
        reinterpret_cast<PFN_vkQueueSubmit>(vkGetDeviceProcAddr(device, "vkQueueSubmit"));
    PFN_vkQueueWaitIdle vkQueueWaitIdle =
        reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetDeviceProcAddr(device, "vkQueueWaitIdle"));
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers =
        reinterpret_cast<PFN_vkFreeCommandBuffers>(vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"));
    PFN_vkCreateCommandPool vkCreateCommandPool =
        reinterpret_cast<PFN_vkCreateCommandPool>(vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
    PFN_vkDestroyCommandPool vkDestroyCommandPool =
        reinterpret_cast<PFN_vkDestroyCommandPool>(vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
    // For implicit sync: create a binary semaphore exportable as a sync_file
    // (mirrors wlroots pass.c:485-494). vkGetSemaphoreFdKHR is provided by
    // VK_KHR_external_semaphore_fd, which wlroots enables on the device
    // (vulkan.c:506-635); RADV on amdgpu supports it.
    PFN_vkCreateSemaphore vkCreateSemaphore =
        reinterpret_cast<PFN_vkCreateSemaphore>(vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
    PFN_vkDestroySemaphore vkDestroySemaphore =
        reinterpret_cast<PFN_vkDestroySemaphore>(vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
    PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR =
        reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));

    if (!vkAllocateCommandBuffers || !vkBeginCommandBuffer || !vkCmdPipelineBarrier ||
        !vkEndCommandBuffer || !vkQueueSubmit || !vkQueueWaitIdle ||
        !vkFreeCommandBuffers || !vkCreateCommandPool || !vkDestroyCommandPool ||
        !vkCreateSemaphore || !vkDestroySemaphore || !vkGetSemaphoreFdKHR) {
        qCWarning(lcWlRenderHelper) << "Vulkan: required command/sync functions unavailable for layout transition";
        return;
    }

    // Create a transient command pool for the graphics queue family.
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = handles->gfxQueueFamilyIdx;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper) << "Vulkan: vkCreateCommandPool failed for layout transition, error=" << res;
        return;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    res = vkAllocateCommandBuffers(device, &allocInfo, &cb);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper) << "Vulkan: vkAllocateCommandBuffers failed for layout transition, error=" << res;
        vkDestroyCommandPool(device, commandPool, nullptr);
        return;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &beginInfo);

    // NOTE: No layout barrier is recorded here. The previous approach
    // (transition COLOR_ATTACHMENT_OPTIMAL -> GENERAL + setNativeLayout) caused
    // InvalidImageLayout validation errors (13.txt) because Qt RHI does not
    // update usageState.layout after its render pass (finalLayout stays
    // COLOR_ATTACHMENT_OPTIMAL but the tracked value goes stale), and
    // preserveColorContents mode expects COLOR_ATTACHMENT_OPTIMAL as
    // initialLayout. KMS scanout reads the dmabuf's physical memory directly
    // and does not care about the Vulkan image layout, so leaving the image in
    // COLOR_ATTACHMENT_OPTIMAL is acceptable. This command buffer is empty and
    // serves only as a submit载体 to signal the semaphore for sync_file export.

    vkEndCommandBuffer(cb);

    // Create a binary semaphore exportable as a sync_file (VK_KHR_external_
    // semaphore_fd). Signalled by the submit below, then exported to a
    // sync_file fd and imported into the dmabuf so KMS implicit sync waits
    // for the Vulkan render. Mirrors wlroots pass.c:485-494 (creation) and
    // renderer.c:994-1026 (export + dmabuf_import_sync_file).
    VkExportSemaphoreCreateInfo exportInfo = {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &exportInfo;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    res = vkCreateSemaphore(device, &semInfo, nullptr, &semaphore);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper) << "Vulkan: vkCreateSemaphore failed for sync, error=" << res;
        vkFreeCommandBuffers(device, commandPool, 1, &cb);
        vkDestroyCommandPool(device, commandPool, nullptr);
        return;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphore;
    res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper) << "Vulkan: vkQueueSubmit failed for layout transition, error=" << res;
    } else {
        // Wait for the submitted command buffer (layout transition) to complete
        // before exporting the sync_file and destroying the semaphore/command
        // buffer. Unlike wlroots (which uses timeline semaphores with
        // vkQueueSubmit2KHR and relies on vkGetSemaphoreFdKHR's implicit wait),
        // our binary semaphore + vkQueueSubmit path requires an explicit
        // vkQueueWaitIdle — the validation layer (12.txt) confirmed that
        // vkDestroySemaphore/vkFreeCommandBuffers were called while the
        // semaphore/command buffer was still pending.
        vkQueueWaitIdle(queue);

        // Export the signalled semaphore as a sync_file fd. After vkQueueWaitIdle
        // the semaphore is signalled and the GPU is idle, so this returns an
        // already-signalled sync_file fd and resets the semaphore.
        VkSemaphoreGetFdInfoKHR getFdInfo = {};
        getFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        getFdInfo.semaphore = semaphore;
        getFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
        int syncFileFd = -1;
        VkResult fdRes = vkGetSemaphoreFdKHR(device, &getFdInfo, &syncFileFd);
        if (fdRes != VK_SUCCESS || syncFileFd < 0) {
            qCWarning(lcWlRenderHelper) << "Vulkan: vkGetSemaphoreFdKHR failed, error=" << fdRes
                                        << "- dmabuf implicit sync not signalled (KMS may reject commit)";
        } else {
            // The GPU has completed (vkQueueWaitIdle above). Signal the dmabuf's
            // implicit sync fence on every plane so KMS scanout waits for the
            // Vulkan render. Mirrors wlroots renderer.c:1014-1026
            // (dmabuf_import_sync_file with DMA_BUF_SYNC_WRITE).
            // DMA_BUF_IOCTL_IMPORT_SYNC_FILE is a kernel UAPI (linux/dma-buf.h,
            // available since Linux 5.20).
            //
            // NOTE: qw_buffer::get_dmabuf() returns a *reference* to the
            // buffer's dmabuf attributes (gbm's buffer_get_dmabuf does a
            // shallow struct copy, no fd dup). wlr_dmabuf_attributes_finish()
            // must NOT be called — it would close the buffer's own fds.
            wlr_dmabuf_attributes dmabuf;
            if (buffer->get_dmabuf(&dmabuf)) {
                for (int i = 0; i < dmabuf.n_planes; ++i) {
                    struct dma_buf_import_sync_file data = {};
                    data.flags = DMA_BUF_SYNC_WRITE;
                    data.fd = syncFileFd;
                    if (ioctl(dmabuf.fd[i], DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &data) != 0) {
                        qCWarning(lcWlRenderHelper) << "Vulkan: DMA_BUF_IOCTL_IMPORT_SYNC_FILE failed on plane" << i
                                                    << "errno=" << errno << "- KMS implicit sync may not wait";
                        break;
                    }
                }
            } else {
                qCWarning(lcWlRenderHelper) << "Vulkan: output buffer has no dmabuf, cannot signal implicit sync";
            }
            close(syncFileFd);
        }
    }

    vkDestroySemaphore(device, semaphore, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &cb);
    vkDestroyCommandPool(device, commandPool, nullptr);
#else
    Q_UNUSED(rhi);
    Q_UNUSED(texture);
    Q_UNUSED(buffer);
#endif
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
    // The wlroots renderer type is determined by WLR_RENDERER. treeland pairs
    // the Vulkan wlroots renderer with Qt RHI Vulkan; the OpenGL branch below
    // keeps the historical bridge available for non-treeland callers.
    const auto wlrRenderer = qgetenv("WLR_RENDERER");
    switch (api) {
    case QSGRendererInterface::OpenGL:
#ifdef ENABLE_VULKAN_RENDER
        if (wlrRenderer == "vulkan") {
            renderer = createRendererWithType("vulkan", backend);
            Q_ASSERT(!renderer || renderer->is_vk());
            break;
        }
#endif
        renderer = createRendererWithType("gles2", backend);
        Q_ASSERT(!renderer || renderer->is_gles2());
        break;
#ifdef ENABLE_VULKAN_RENDER
    case QSGRendererInterface::Vulkan: {
        renderer = createRendererWithType("vulkan", backend);
        if (renderer && !renderer->is_vk()) {
            qCWarning(lcWlRenderHelper) << "Vulkan: wlr_renderer was created but is not a Vulkan renderer, rendering will likely fail";
        }
        Q_ASSERT(!renderer || renderer->is_vk());
        break;
    }
#endif
    case QSGRendererInterface::Software:
        renderer = createRendererWithType("pixman", backend);
        Q_ASSERT(!renderer || renderer->is_pixman());
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
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    } else if (wlrRenderer == "vulkan") {
#ifdef ENABLE_VULKAN_RENDER
        qunsetenv("QSG_RHI_BACKEND");
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
        qCInfo(lcWlRenderHelper)
            << "Vulkan wlroots renderer requested: forcing treeland Qt Quick RHI to Vulkan"
            << "and clearing QSG_RHI_BACKEND for child processes";
#else
        qFatal("Vulkan support is not enabled");
#endif
    } else if (wlrRenderer == "pixman") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    } else {
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

static void updateGLTexture(QRhi *rhi, qw_texture *handle, QSGPlainTexture *texture) {
    wlr_gles2_texture_attribs attribs;
    handle->get_attribs(&attribs);
    QSize size(handle->handle()->width, handle->handle()->height);

#define GL_TEXTURE_EXTERNAL_OES           0x8D65
    QQuickWindowPrivate::TextureFromNativeTextureFlags flags = attribs.target == GL_TEXTURE_EXTERNAL_OES
                                                                   ? QQuickWindowPrivate::NativeTextureIsExternalOES
                                                                   : QQuickWindowPrivate::TextureFromNativeTextureFlags {};
    texture->setOwnsTexture(false);
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
static void updateVKTexture(QRhi *rhi, qw_texture *handle, QSGPlainTexture *texture) {
    wlr_vk_image_attribs attribs;
    handle->get_image_attribs(&attribs);
    QSize size(handle->handle()->width, handle->handle()->height);

    texture->setOwnsTexture(false);
    texture->setTextureFromNativeTexture(rhi,
                                         vkimage_cast(attribs.image),
                                         attribs.layout, attribs.format, size,
                                         {}, {});
    texture->setHasAlphaChannel(handle->has_alpha());
    texture->setTextureSize(size);
}
#endif

#ifdef ENABLE_VULKAN_RENDER
// (updateEglDmabufTexture removed - logic inlined into makeTexture)

static bool bufferExportsDmabuf(qw_buffer *buffer)
{
    if (!buffer || !buffer->handle())
        return false;

    wlr_dmabuf_attributes dmabuf = {};
    return buffer->get_dmabuf(&dmabuf);
}

static bool envFlagExplicitlyEnabled(const char *name)
{
    const QByteArray value = qgetenv(name).trimmed().toLower();
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

static bool envFlagExplicitlyDisabled(const char *name)
{
    const QByteArray value = qgetenv(name).trimmed().toLower();
    return value == "0" || value == "false" || value == "no" || value == "off";
}

static bool vulkanNonDmabufDiagnosticsEnabled()
{
    static const bool enabled = envFlagExplicitlyEnabled("WAYLIB_VK_NON_DMABUF_DIAGNOSTICS")
        || envFlagExplicitlyEnabled("TREELAND_VK_NON_DMABUF_DIAGNOSTICS");
    return enabled;
}

static bool vulkanNonDmabufForceOpaqueEnabled()
{
    static const bool enabled = envFlagExplicitlyEnabled("WAYLIB_VK_NON_DMABUF_FORCE_OPAQUE")
        || envFlagExplicitlyEnabled("TREELAND_VK_NON_DMABUF_FORCE_OPAQUE");
    return enabled;
}

static bool vulkanNonDmabufForceReadbackEnabled()
{
    static const bool enabled = envFlagExplicitlyEnabled("WAYLIB_VK_NON_DMABUF_FORCE_READBACK")
        || envFlagExplicitlyEnabled("TREELAND_VK_NON_DMABUF_FORCE_READBACK");
    return enabled;
}

static bool vulkanNonDmabufPartialUploadEnabled()
{
    static const bool enabled = envFlagExplicitlyEnabled("WAYLIB_VK_NON_DMABUF_PARTIAL_UPLOAD")
        || envFlagExplicitlyEnabled("TREELAND_VK_NON_DMABUF_PARTIAL_UPLOAD");
    return enabled;
}

static bool vulkanUnsafeReadbackEnabled()
{
    static const bool enabled = envFlagExplicitlyEnabled("WAYLIB_VK_ALLOW_UNSAFE_READBACK")
        || envFlagExplicitlyEnabled("TREELAND_VK_ALLOW_UNSAFE_READBACK");
    return enabled;
}

struct Q_DECL_HIDDEN VulkanTextureImageSample {
    int samples = 0;
    int minAlpha = 0;
    int maxAlpha = 0;
    int nonZeroAlpha = 0;
    int opaqueAlpha = 0;
    int nonZeroColor = 0;
    int maxColor = 0;
    QRgb topLeft = 0;
    QRgb center = 0;
    QRgb bottomRight = 0;
};

static VulkanTextureImageSample sampleVulkanTextureImage(const QImage &image)
{
    VulkanTextureImageSample sample;
    if (image.isNull())
        return sample;

    sample.minAlpha = 255;
    sample.topLeft = image.pixel(0, 0);
    sample.center = image.pixel(image.width() / 2, image.height() / 2);
    sample.bottomRight = image.pixel(image.width() - 1, image.height() - 1);

    const int xStep = std::max(1, image.width() / 64);
    const int yStep = std::max(1, image.height() / 64);
    for (int y = 0; y < image.height(); y += yStep) {
        for (int x = 0; x < image.width(); x += xStep) {
            const QRgb pixel = image.pixel(x, y);
            const int alpha = qAlpha(pixel);
            const int maxChannel = std::max({ qRed(pixel), qGreen(pixel), qBlue(pixel) });
            sample.minAlpha = std::min(sample.minAlpha, alpha);
            sample.maxAlpha = std::max(sample.maxAlpha, alpha);
            sample.maxColor = std::max(sample.maxColor, maxChannel);
            ++sample.samples;
            if (alpha > 0)
                ++sample.nonZeroAlpha;
            if (alpha == 255)
                ++sample.opaqueAlpha;
            if (maxChannel > 0)
                ++sample.nonZeroColor;
        }
    }

    if (sample.samples == 0)
        sample.minAlpha = 0;

    return sample;
}

static void logVulkanTextureImageDiagnostics(const char *source, const QImage &image,
                                             uint32_t drmFormat, qsizetype stride,
                                             bool sourceHasAlpha, bool forcedOpaque)
{
    if (!vulkanNonDmabufDiagnosticsEnabled())
        return;

    const auto sample = sampleVulkanTextureImage(image);
    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI client texture image diagnostics"
        << "source" << source
        << "size" << image.size()
        << "imageFormat" << image.format()
        << "drmFormat" << drmFormatToName(drmFormat)
        << "stride" << stride
        << "sourceAlpha" << sourceHasAlpha
        << "forcedOpaque" << forcedOpaque
        << "samples" << sample.samples
        << "alphaRange" << sample.minAlpha << "-" << sample.maxAlpha
        << "nonZeroAlpha" << sample.nonZeroAlpha
        << "opaqueAlpha" << sample.opaqueAlpha
        << "nonZeroColor" << sample.nonZeroColor
        << "maxColor" << sample.maxColor
        << "topLeft" << Qt::hex << quint32(sample.topLeft)
        << "center" << quint32(sample.center)
        << "bottomRight" << quint32(sample.bottomRight) << Qt::dec;
}

static bool vulkanNonDmabufAlphaLooksInvalidOpaque(uint32_t drmFormat,
                                                   bool sourceHasAlpha,
                                                   const VulkanTextureImageSample &sample)
{
    return sourceHasAlpha
        && drmFormatLikelyHasAlpha(drmFormat)
        && sample.samples > 0
        && sample.maxAlpha == 0
        && sample.nonZeroColor > 0;
}

static QImage forceOpaqueVulkanTextureImage(QImage image)
{
    if (image.isNull() || image.format() == QImage::Format_RGB32)
        return image;

    image = image.convertToFormat(QImage::Format_RGB32);
    image.setDevicePixelRatio(1.0);
    return image;
}

static qint64 rectArea(const QRect &rect)
{
    return qint64(rect.width()) * rect.height();
}

struct Q_DECL_HIDDEN VulkanBufferUploadRegions {
    QVarLengthArray<QRect, 16> rects;
    QRect bounds;
    int inputRectCount = 0;
    bool usedFullBuffer = false;
    bool usedUnion = false;
    const char *reason = "unknown";

    qint64 uploadArea() const {
        qint64 area = 0;
        for (const auto &rect : rects)
            area += rectArea(rect);
        return area;
    }
};

static VulkanBufferUploadRegions vulkanUploadRegionsFromSurfaceDamage(wlr_surface *surface,
                                                                      const QSize &bufferSize,
                                                                      bool forceFullBuffer)
{
    VulkanBufferUploadRegions result;
    const QRect bufferRect(QPoint(0, 0), bufferSize);
    if (bufferRect.isEmpty()) {
        result.reason = "invalid-buffer-size";
        return result;
    }

    auto useFullBuffer = [&result, bufferRect](const char *reason) {
        result.rects.clear();
        result.rects.append(bufferRect);
        result.bounds = bufferRect;
        result.inputRectCount = 1;
        result.usedFullBuffer = true;
        result.reason = reason;
    };

    if (forceFullBuffer) {
        useFullBuffer("new-or-resized-upload-texture");
        return result;
    }

    if (!surface) {
        useFullBuffer("missing-surface-damage");
        return result;
    }

    const auto *bufferDamage = qw_surface::from(surface)->buffer_damage();
    if (!bufferDamage || !pixman_region32_not_empty(bufferDamage)) {
        useFullBuffer("empty-surface-buffer-damage");
        return result;
    }

    int rectCount = 0;
    const pixman_box32_t *boxes = pixman_region32_rectangles(bufferDamage, &rectCount);
    result.inputRectCount = rectCount;
    if (!boxes || rectCount <= 0) {
        useFullBuffer("unreadable-surface-buffer-damage");
        return result;
    }

    QRect bounds;
    for (int i = 0; i < rectCount; ++i) {
        QRect rect(QPoint(boxes[i].x1, boxes[i].y1),
                   QPoint(boxes[i].x2 - 1, boxes[i].y2 - 1));
        rect = rect.intersected(bufferRect);
        if (rect.isEmpty())
            continue;

        result.rects.append(rect);
        bounds = bounds.isNull() ? rect : bounds.united(rect);
    }

    if (result.rects.isEmpty()) {
        result.reason = "surface-buffer-damage-outside-buffer";
        return result;
    }

    result.bounds = bounds;
    result.reason = "surface-buffer-damage";

    static constexpr int maxIndividualUploadRects = 16;
    if (result.rects.size() > maxIndividualUploadRects) {
        result.rects.clear();
        result.rects.append(bounds);
        result.usedUnion = true;
        result.reason = "surface-buffer-damage-union";
    }

    return result;
}

static QRhiTexture::Format rhiTextureFormatForVulkanUpload(QRhi *rhi,
                                                           QImage::Format imageFormat,
                                                           bool *usesBgra)
{
    if (usesBgra)
        *usesBgra = false;

    switch (imageFormat) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        if (rhi && rhi->isTextureFormatSupported(QRhiTexture::BGRA8)) {
            if (usesBgra)
                *usesBgra = true;
            return QRhiTexture::BGRA8;
        }
        return QRhiTexture::UnknownFormat;
    case QImage::Format_RGBX8888:
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return QRhiTexture::RGBA8;
    default:
        return QRhiTexture::UnknownFormat;
    }
}

static bool updateVulkanTextureFromBufferDataWithRhiUpload(QRhi *rhi,
                                                           QRhiCommandBuffer *commandBuffer,
                                                           QSGPlainTexture *texture,
                                                           const QImage &wrapped,
                                                           const QSize &size,
                                                           uint32_t drmFormat,
                                                           qsizetype stride,
                                                           bool sourceHasAlpha,
                                                           bool forcedOpaque,
                                                           wlr_surface *surface,
                                                           qw_buffer *buffer,
                                                           qw_texture *handle,
                                                           const VulkanTextureImageSample &sample)
{
    if (!vulkanNonDmabufPartialUploadEnabled() || !rhi || !commandBuffer
        || !texture || wrapped.isNull()) {
        return false;
    }

    bool usesBgra = false;
    const QRhiTexture::Format textureFormat =
        rhiTextureFormatForVulkanUpload(rhi, wrapped.format(), &usesBgra);
    if (textureFormat == QRhiTexture::UnknownFormat) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI non-dmabuf partial upload unavailable:"
            << "unsupported image format"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << size
            << "imageFormat" << wrapped.format()
            << "drmFormat" << drmFormatToName(drmFormat);
        return false;
    }

    QRhiTexture *rhiTexture = texture->rhiTexture();
    QRhiTexture::Flags textureFlags;
    const bool wantsMipmaps = texture->mipmapFiltering() != QSGTexture::None;
    if (wantsMipmaps)
        textureFlags |= QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
    const bool canReuseTexture =
        texture->ownsTexture()
        && rhiTexture
        && rhiTexture->pixelSize() == size
        && rhiTexture->format() == textureFormat
        && rhiTexture->flags() == textureFlags;
    const bool rebuildTexture = !canReuseTexture;

    if (rebuildTexture) {
        auto *newTexture = rhi->newTexture(textureFormat, size, 1, textureFlags);
        if (!newTexture || !newTexture->create()) {
            qCWarning(lcWlRenderHelper)
                << "Vulkan RHI non-dmabuf upload texture creation failed"
                << "buffer" << buffer
                << "texture" << handle
                << "size" << size
                << "format" << drmFormatToName(drmFormat)
                << "rhiFormat" << textureFormat;
            delete newTexture;
            return false;
        }

        if (texture->rhiTexture() && !texture->ownsTexture())
            texture->setTexture(nullptr);

        texture->setOwnsTexture(true);
        texture->setTexture(newTexture);
        rhiTexture = newTexture;
    }

    const VulkanBufferUploadRegions regions =
        vulkanUploadRegionsFromSurfaceDamage(surface, size, rebuildTexture);
    if (regions.rects.isEmpty()) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI non-dmabuf upload skipped because damage is empty after clipping"
            << "buffer" << buffer
            << "texture" << handle
            << "surface" << surface
            << "size" << size
            << "damageRects" << regions.inputRectCount
            << "reason" << regions.reason
            << "hasExistingTexture" << bool(rhiTexture);
        texture->setHasAlphaChannel(sourceHasAlpha && !forcedOpaque);
        texture->setTextureSize(size);
        return bool(rhiTexture);
    }

    QVarLengthArray<QRhiTextureUploadEntry, 16> entries;
    entries.reserve(regions.rects.size());
    qint64 copiedBytes = 0;
    for (const auto &rect : regions.rects) {
        QImage image = wrapped.copy(rect);
        if (forcedOpaque)
            image = forceOpaqueVulkanTextureImage(std::move(image));

        if (image.isNull())
            continue;

        copiedBytes += image.sizeInBytes();
        QRhiTextureSubresourceUploadDescription subres(image);
        subres.setDestinationTopLeft(rect.topLeft());
        entries.append(QRhiTextureUploadEntry(0, 0, subres));
    }

    if (entries.isEmpty()) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI non-dmabuf upload unavailable: all dirty rect copies were empty"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << size
            << "dirtyRects" << regions.rects.size();
        return false;
    }

    auto *resourceUpdates = rhi->nextResourceUpdateBatch();
    QRhiTextureUploadDescription uploadDescription;
    uploadDescription.setEntries(entries.begin(), entries.end());
    resourceUpdates->uploadTexture(rhiTexture, uploadDescription);
    if (wantsMipmaps)
        resourceUpdates->generateMips(rhiTexture);
    commandBuffer->resourceUpdate(resourceUpdates);

    texture->setOwnsTexture(true);
    texture->setTexture(rhiTexture);
    texture->setHasAlphaChannel(sourceHasAlpha && !forcedOpaque);
    texture->setTextureSize(size);

    const qint64 bufferArea = rectArea(QRect(QPoint(0, 0), size));
    const qint64 uploadArea = regions.uploadArea();
    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI client texture uploaded from buffer data"
        << "mode" << (rebuildTexture ? "rhi-full-upload" : "rhi-damage-upload")
        << "buffer" << buffer
        << "texture" << handle
        << "surface" << surface
        << "size" << size
        << "format" << drmFormatToName(drmFormat)
        << "imageFormat" << wrapped.format()
        << "rhiFormat" << textureFormat
        << "usesBgra" << usesBgra
        << "stride" << stride
        << "alpha" << texture->hasAlphaChannel()
        << "forcedOpaque" << forcedOpaque
        << "createdTexture" << rebuildTexture
        << "damageRects" << regions.inputRectCount
        << "uploadRects" << regions.rects.size()
        << "usedFullBuffer" << regions.usedFullBuffer
        << "usedUnion" << regions.usedUnion
        << "uploadReason" << regions.reason
        << "uploadBounds" << regions.bounds
        << "uploadArea" << uploadArea
        << "bufferArea" << bufferArea
        << "uploadPermille" << (bufferArea > 0 ? uploadArea * 1000 / bufferArea : 0)
        << "copiedBytes" << copiedBytes
        << "sampleAlphaRange" << sample.minAlpha << "-" << sample.maxAlpha
        << "sampleNonZeroColor" << sample.nonZeroColor
        << "bufferExportsDmabuf" << false;
    return true;
}

static bool updateVulkanTextureFromImage(QSGPlainTexture *texture, QImage image,
                                         bool hasAlpha)
{
    if (image.isNull())
        return false;

    if (texture->rhiTexture() && !texture->ownsTexture())
        texture->setTexture(nullptr);

    const QSize size = image.size();
    texture->setOwnsTexture(true);
    texture->setImage(std::move(image));
    texture->setHasAlphaChannel(hasAlpha);
    texture->setTextureSize(size);
    return true;
}

static bool updateVulkanTextureFromBufferData(qw_buffer *buffer, qw_texture *handle,
                                              QSGPlainTexture *texture,
                                              wlr_surface *surface,
                                              QRhi *rhi,
                                              QRhiCommandBuffer *commandBuffer)
{
    if (!buffer || !buffer->handle() || !handle || !handle->handle() || !texture)
        return false;

    QElapsedTimer elapsed;
    elapsed.start();

    void *data = nullptr;
    uint32_t drmFormat = DRM_FORMAT_INVALID;
    size_t stride = 0;
    if (!buffer->begin_data_ptr_access(WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                       &data, &drmFormat, &stride)) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture buffer-data upload skipped:"
            << "data pointer access unavailable"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << QSize(buffer->handle()->width, buffer->handle()->height)
            << "surface" << surface
            << "commandBuffer" << commandBuffer
            << "bufferExportsDmabuf" << bufferExportsDmabuf(buffer);
        return false;
    }

    const QSize size(buffer->handle()->width, buffer->handle()->height);
    const QImage::Format imageFormat = WTools::toImageFormat(drmFormat);
    bool updated = false;
    bool forcedOpaque = false;
    const bool sourceHasAlpha = handle->has_alpha();
    const bool forceSurfaceOpaque = surfaceOpaqueRegionCoversBuffer(surface, buffer);
    if (!size.isEmpty() && data && stride > 0 && imageFormat != QImage::Format_Invalid) {
        const QImage wrapped(static_cast<const uchar *>(data),
                             size.width(), size.height(), qsizetype(stride), imageFormat);
        const auto sample = sampleVulkanTextureImage(wrapped);
        logVulkanTextureImageDiagnostics("buffer-data", wrapped, drmFormat,
                                         qsizetype(stride), sourceHasAlpha, false);
        const bool alphaLooksInvalidOpaque =
            vulkanNonDmabufAlphaLooksInvalidOpaque(drmFormat, sourceHasAlpha, sample);
        forcedOpaque = forceSurfaceOpaque
            || vulkanNonDmabufForceOpaqueEnabled()
            || alphaLooksInvalidOpaque;
        if (alphaLooksInvalidOpaque) {
            qCDebug(lcWlRenderHelper)
                << "Vulkan RHI client texture treating invalid zero-alpha buffer data as opaque"
                << "buffer" << buffer
                << "texture" << handle
                << "size" << size
                << "format" << drmFormatToName(drmFormat)
                << "stride" << stride
                << "alphaRange" << sample.minAlpha << "-" << sample.maxAlpha
                << "nonZeroColor" << sample.nonZeroColor
                << "maxColor" << sample.maxColor
                << "surfaceOpaqueFull" << forceSurfaceOpaque;
        }

        updated = updateVulkanTextureFromBufferDataWithRhiUpload(rhi, commandBuffer,
                                                                 texture, wrapped, size,
                                                                 drmFormat, qsizetype(stride),
                                                                 sourceHasAlpha, forcedOpaque,
                                                                 surface, buffer, handle,
                                                                 sample);
        if (!updated) {
            QImage image = wrapped.copy();
            if (forcedOpaque) {
                image = forceOpaqueVulkanTextureImage(std::move(image));
                logVulkanTextureImageDiagnostics("buffer-data-forced-opaque", image,
                                                 drmFormat, qsizetype(image.bytesPerLine()),
                                                 sourceHasAlpha, true);
            }

            updated = updateVulkanTextureFromImage(texture, std::move(image),
                                                   sourceHasAlpha && !forcedOpaque);
            if (updated) {
                qCDebug(lcWlRenderHelper)
                    << "Vulkan RHI client texture fell back to full QSGPlainTexture image upload"
                    << "buffer" << buffer
                    << "texture" << handle
                    << "surface" << surface
                    << "size" << size
                    << "format" << drmFormatToName(drmFormat)
                    << "imageFormat" << imageFormat
                    << "stride" << stride
                    << "hasCommandBuffer" << bool(commandBuffer)
                    << "partialUploadEnabled" << vulkanNonDmabufPartialUploadEnabled();
            }
        }
    }

    buffer->end_data_ptr_access();

    if (updated) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture uploaded from buffer data"
            << "mode" << "buffer-data-final"
            << "buffer" << buffer
            << "texture" << handle
            << "surface" << surface
            << "size" << size
            << "format" << drmFormatToName(drmFormat)
            << "stride" << stride
            << "alpha" << texture->hasAlphaChannel()
            << "forcedOpaque" << forcedOpaque
            << "surfaceOpaqueFull" << forceSurfaceOpaque
            << "bufferExportsDmabuf" << false
            << "elapsedUs" << elapsed.nsecsElapsed() / 1000;
    } else {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture buffer-data upload unavailable"
            << "buffer" << buffer
            << "texture" << handle
            << "surface" << surface
            << "size" << size
            << "format" << drmFormatToName(drmFormat)
            << "stride" << stride
            << "imageFormat" << imageFormat
            << "elapsedUs" << elapsed.nsecsElapsed() / 1000;
    }

    return updated;
}

static bool readVulkanTextureToImage(qw_texture *handle, QSGPlainTexture *texture,
                                     uint32_t drmFormat, bool forceSurfaceOpaque)
{
    if (!handle || !handle->handle() || !texture)
        return false;

    const QSize size(handle->handle()->width, handle->handle()->height);
    const QImage::Format imageFormat = WTools::toImageFormat(drmFormat);
    if (size.isEmpty() || imageFormat == QImage::Format_Invalid)
        return false;

    QImage image(size, imageFormat);
    if (image.isNull())
        return false;

    wlr_texture_read_pixels_options options = {};
    options.data = image.bits();
    options.format = drmFormat;
    options.stride = uint32_t(image.bytesPerLine());
    if (!handle->read_pixels(&options))
        return false;

    const bool sourceHasAlpha = handle->has_alpha();
    const auto sample = sampleVulkanTextureImage(image);
    logVulkanTextureImageDiagnostics("readback", image, drmFormat,
                                     qsizetype(image.bytesPerLine()),
                                     sourceHasAlpha, false);

    const bool alphaLooksInvalidOpaque =
        vulkanNonDmabufAlphaLooksInvalidOpaque(drmFormat, sourceHasAlpha, sample);
    const bool forcedOpaque = forceSurfaceOpaque
        || vulkanNonDmabufForceOpaqueEnabled()
        || alphaLooksInvalidOpaque;
    if (alphaLooksInvalidOpaque) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture treating invalid zero-alpha readback as opaque"
            << "texture" << handle
            << "size" << size
            << "format" << drmFormatToName(drmFormat)
            << "alphaRange" << sample.minAlpha << "-" << sample.maxAlpha
            << "nonZeroColor" << sample.nonZeroColor
            << "maxColor" << sample.maxColor
            << "surfaceOpaqueFull" << forceSurfaceOpaque;
    }
    if (forcedOpaque) {
        image = forceOpaqueVulkanTextureImage(std::move(image));
        logVulkanTextureImageDiagnostics("readback-forced-opaque", image,
                                         drmFormat, qsizetype(image.bytesPerLine()),
                                         sourceHasAlpha, true);
        return updateVulkanTextureFromImage(texture, std::move(image), false);
    }

    return updateVulkanTextureFromImage(texture,
                                        std::move(image),
                                        sourceHasAlpha);
}

static bool updateVulkanTextureFromReadback(qw_buffer *buffer, qw_texture *handle,
                                            QSGPlainTexture *texture,
                                            wlr_surface *surface)
{
    if (!handle || !handle->handle() || !texture)
        return false;

    if (handle->is_vk() && !vulkanUnsafeReadbackEnabled()) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture readback skipped:"
            << "wlroots Vulkan texture read_pixels has unsafe layout ownership for live client textures"
            << "buffer" << buffer
            << "texture" << handle
            << "surface" << surface
            << "size" << QSize(handle->handle()->width, handle->handle()->height)
            << "bufferExportsDmabuf" << bufferExportsDmabuf(buffer)
            << "enableOverride" << "WAYLIB_VK_ALLOW_UNSAFE_READBACK=1";
        return false;
    }

    if (bufferExportsDmabuf(buffer)
        && !waitDmabufImplicitFence(buffer, DMA_BUF_SYNC_READ,
                                   "Vulkan RHI client texture readback",
                                   "implicit acquire")) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI client texture readback rejected:"
            << "implicit producer fence wait failed"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << QSize(handle->handle()->width, handle->handle()->height);
        return false;
    }

    const uint32_t preferredFormat = handle->preferred_read_format();
    const bool forceSurfaceOpaque = surfaceOpaqueRegionCoversBuffer(surface, buffer);
    const uint32_t fallbackFormats[] = {
        preferredFormat,
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,
    };

    const size_t fallbackFormatCount = sizeof(fallbackFormats) / sizeof(fallbackFormats[0]);
    for (size_t i = 0; i < fallbackFormatCount; ++i) {
        const uint32_t drmFormat = fallbackFormats[i];
        if (drmFormat == DRM_FORMAT_INVALID)
            continue;

        bool alreadyTried = false;
        for (size_t j = 0; j < i; ++j) {
            if (fallbackFormats[j] == drmFormat) {
                alreadyTried = true;
                break;
            }
        }
        if (alreadyTried)
            continue;

        if (!readVulkanTextureToImage(handle, texture, drmFormat, forceSurfaceOpaque))
            continue;

        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture uploaded from wlroots readback"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << QSize(handle->handle()->width, handle->handle()->height)
            << "format" << drmFormatToName(drmFormat)
            << "alpha" << texture->hasAlphaChannel()
            << "surfaceOpaqueFull" << forceSurfaceOpaque;
        return true;
    }

    qCWarning(lcWlRenderHelper)
        << "Vulkan RHI client texture readback fallback failed"
        << "buffer" << buffer
        << "texture" << handle
        << "size" << QSize(handle->handle()->width, handle->handle()->height)
        << "preferredFormat" << drmFormatToName(preferredFormat);
    return false;
}

static bool updateVulkanTextureFromNonDmabufBuffer(qw_buffer *buffer, qw_texture *handle,
                                                   QSGPlainTexture *texture,
                                                   wlr_surface *surface,
                                                   QRhi *rhi,
                                                   QRhiCommandBuffer *commandBuffer)
{
    if (!buffer || bufferExportsDmabuf(buffer))
        return false;

    if (!vulkanNonDmabufForceReadbackEnabled()
        && updateVulkanTextureFromBufferData(buffer, handle, texture, surface,
                                            rhi, commandBuffer)) {
        return true;
    }

    if (vulkanNonDmabufForceReadbackEnabled()) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI non-dmabuf client texture buffer-data upload skipped by diagnostics env"
            << "buffer" << buffer
            << "texture" << handle
            << "surface" << surface
            << "size" << QSize(handle->handle()->width, handle->handle()->height)
            << "unsafeReadbackEnabled" << vulkanUnsafeReadbackEnabled();
    }

    return updateVulkanTextureFromReadback(buffer, handle, texture, surface);
}
#endif

static void updateImage(QRhi *, qw_texture *handle, QSGPlainTexture *texture) {
    auto image = handle->get_image();
    if (texture->rhiTexture() && !texture->ownsTexture())
        texture->setTexture(nullptr);
    texture->setOwnsTexture(true);
    texture->setImage(WTools::fromPixmanImage(image));
}

typedef void(*UpdateTextureFunction)(QRhi *, qw_texture *, QSGPlainTexture *);

static UpdateTextureFunction getUpdateTextFunction(qw_texture *handle)
{
    const auto api = WRenderHelper::getGraphicsApi();
    if (api == QSGRendererInterface::OpenGL) {
        if (handle->is_gles2()) {
            return updateGLTexture;
        }
#ifdef ENABLE_VULKAN_RENDER
        // Vulkan wlroots renderer with GL Qt RHI: handled separately in
        // makeTexture() via updateEglDmabufTexture (needs buffer access).
#endif
        return nullptr;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (api == QSGRendererInterface::Vulkan) {
        Q_ASSERT(handle->is_vk());
        return updateVKTexture;
    }
#endif
    else if (api == QSGRendererInterface::Software) {
        Q_ASSERT(handle->is_pixman());
        return updateImage;
    }

    return nullptr;
}

bool WRenderHelper::makeTexture(QRhi *rhi, qw_texture *handle,
                                QSGPlainTexture *texture, qw_buffer *buffer,
                                NativeTextureCleanup *nativeCleanup,
                                bool allowBufferDirectImport,
                                wlr_surface *surface,
                                QRhiCommandBuffer *commandBuffer)
{
#ifdef ENABLE_VULKAN_RENDER
    // Vulkan renderer + GL RHI: import the buffer's dmabuf as a GL texture
    // via EGL. This is the client-surface counterpart of acquireRenderTarget.
    // If EGL import fails (EGL_BAD_ALLOC on some modifiers), return false
    // gracefully — the client window won't display but the system stays
    // stable (no commit failure, no crash).
    if (WRenderHelper::getGraphicsApi() == QSGRendererInterface::OpenGL
        && handle->is_vk()) {
        QSize size(handle->handle()->width, handle->handle()->height);

        // Try EGL dmabuf import first (for client surfaces with dmabuf buffers).
        if (allowBufferDirectImport && buffer) {
            wlr_dmabuf_attributes dmabuf;
            if (buffer->get_dmabuf(&dmabuf)) {
                EGLDisplay eglDisplay = eglGetCurrentDisplay();
                if (eglDisplay != EGL_NO_DISPLAY) {
                    EGLImage eglImage = EGL_NO_IMAGE;
                    GLuint glTex = 0;
                    if (eglImportDmabufToGLTexture(eglDisplay, &dmabuf, &eglImage, &glTex)) {
                        // NOTE: do NOT call wlr_dmabuf_attributes_finish() —
                        // qw_buffer::get_dmabuf returns a shallow reference
                        // (no fd dup). finish would close the buffer's own fds.
                        texture->setOwnsTexture(false);
                        texture->setTextureFromNativeTexture(rhi, glTex, 0, 0, size, {},
                                                              QQuickWindowPrivate::TextureFromNativeTextureFlags{});
                        texture->setHasAlphaChannel(handle->has_alpha());
                        texture->setTextureSize(size);
                        if (nativeCleanup) {
                            *nativeCleanup = {
                                NativeTextureCleanup::Type::OpenGLTexture,
                                glTex,
                                reinterpret_cast<void *>(eglImage),
                                reinterpret_cast<void *>(eglDisplay),
                            };
                        }
                        return texture->rhiTexture() != nullptr;
                    }
                }
            }
        }

        // Fallback for shm/pixels textures (e.g. cursor QImage, no dmabuf):
        // read pixels via wlr_texture_read_pixels and upload to a GL texture
        // via glTexImage2D. This mirrors how QSGPlainTexture handles QImage
        // textures in the software/GL path.
        uint32_t fmt = DRM_FORMAT_ARGB8888;
        // Use wlr_texture_preferred_read_format to get the optimal format.
        uint32_t pref = handle->preferred_read_format();
        if (pref != 0)
            fmt = pref;

        int bpp = 4; // ARGB8888 = 4 bytes per pixel
        if (fmt == DRM_FORMAT_ARGB8888 || fmt == DRM_FORMAT_XRGB8888) {
            bpp = 4;
        } else {
            // Unsupported read format for GL upload fallback
            return false;
        }

        const int stride = size.width() * bpp;
        QByteArray pixels(size.height() * stride, 0);

        struct wlr_texture_read_pixels_options options = {};
        options.data = pixels.data();
        options.format = fmt;
        options.stride = stride;
        if (!handle->read_pixels(&options))
            return false;

        GLuint glTex = 0;
        clearGlErrors();
        glGenTextures(1, &glTex);
        if (!glTex)
            return false;

        glBindTexture(GL_TEXTURE_2D, glTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // DRM_FORMAT_ARGB8888 maps to GL_BGRA + GL_UNSIGNED_BYTE on little-endian.
        // QImage::Format_ARGB32 uses the same memory layout.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width(), size.height(), 0,
                     GL_BGRA, GL_UNSIGNED_BYTE, pixels.constData());
        glBindTexture(GL_TEXTURE_2D, 0);
        if (!textureUploadSucceeded()) {
            glDeleteTextures(1, &glTex);
            return false;
        }

        texture->setOwnsTexture(false);
        texture->setTextureFromNativeTexture(rhi, glTex, 0, 0, size, {},
                                              QQuickWindowPrivate::TextureFromNativeTextureFlags{});
        texture->setHasAlphaChannel(handle->has_alpha());
        texture->setTextureSize(size);
        if (nativeCleanup) {
            *nativeCleanup = {
                NativeTextureCleanup::Type::OpenGLTexture,
                glTex,
                nullptr,
                nullptr,
            };
        }
        return texture->rhiTexture() != nullptr;
    }
#endif
    auto updateTexture = getUpdateTextFunction(handle);
#ifdef ENABLE_VULKAN_RENDER
    const bool isVulkanRhiVkTexture = WRenderHelper::getGraphicsApi() == QSGRendererInterface::Vulkan
        && handle->is_vk();
    const bool dmabufBackedBuffer = isVulkanRhiVkTexture && buffer && bufferExportsDmabuf(buffer);
    if (allowBufferDirectImport && isVulkanRhiVkTexture && dmabufBackedBuffer && nativeCleanup) {
        if (WRenderHelper::makeVulkanTextureFromBuffer(rhi, buffer, texture, nativeCleanup,
                                                       surface))
            return true;

        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import unavailable,"
               " trying synchronized fallback paths"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << handle->handle()->width << "x" << handle->handle()->height
            << "allowBufferDirectImport" << allowBufferDirectImport;
    }

    if (isVulkanRhiVkTexture && buffer && !dmabufBackedBuffer) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI client texture using non-dmabuf buffer upload path"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << handle->handle()->width << "x" << handle->handle()->height
            << "allowBufferDirectImport" << allowBufferDirectImport;
    }

    if (isVulkanRhiVkTexture && buffer
        && updateVulkanTextureFromNonDmabufBuffer(buffer, handle, texture, surface,
                                                 rhi, commandBuffer)) {
        return true;
    }

    if (isVulkanRhiVkTexture && buffer && dmabufBackedBuffer
        && updateVulkanTextureFromReadback(buffer, handle, texture, surface)) {
        return true;
    }

    if (isVulkanRhiVkTexture && buffer) {
        if (dmabufBackedBuffer) {
            qCDebug(lcWlRenderHelper)
                << "Vulkan RHI skipped unsafe wlroots native VkImage wrapper for dmabuf-backed texture"
                << "buffer" << buffer
                << "texture" << handle
                << "size" << handle->handle()->width << "x" << handle->handle()->height
                << "allowBufferDirectImport" << allowBufferDirectImport;
            return false;
        }

        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI texture using wlroots native VkImage wrapper"
            << "buffer" << buffer
            << "texture" << handle
            << "size" << handle->handle()->width << "x" << handle->handle()->height
            << "bufferExportsDmabuf" << dmabufBackedBuffer
            << "allowBufferDirectImport" << allowBufferDirectImport;
    }
#endif
    if (Q_UNLIKELY(!updateTexture))
        return false;
    updateTexture(rhi, handle, texture);
    return true;
}

bool WRenderHelper::makeOpenGLTextureFromBuffer(QRhi *rhi, qw_buffer *buffer,
                                                QSGPlainTexture *texture,
                                                NativeTextureCleanup *nativeCleanup)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!rhi || !buffer || !texture || !nativeCleanup)
        return false;

    if (rhi->backend() != QRhi::OpenGLES2)
        return false;

    auto *handle = buffer->handle();
    if (!handle || handle->width <= 0 || handle->height <= 0)
        return false;

    wlr_dmabuf_attributes dmabuf;
    if (!buffer->get_dmabuf(&dmabuf)) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan+GL direct texture import skipped: buffer has no dmabuf"
            << buffer << "size" << handle->width << "x" << handle->height;
        return false;
    }

    EGLDisplay eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan+GL direct texture import failed: no current EGL display"
            << buffer << "size" << handle->width << "x" << handle->height;
        return false;
    }

    EGLImage eglImage = EGL_NO_IMAGE;
    GLuint glTex = 0;
    if (!eglImportDmabufToGLTexture(eglDisplay, &dmabuf, &eglImage, &glTex)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan+GL direct texture import failed"
            << buffer
            << "size" << handle->width << "x" << handle->height
            << "format" << Qt::hex << dmabuf.format << Qt::dec
            << "modifier" << Qt::hex << dmabuf.modifier << Qt::dec
            << "planes" << dmabuf.n_planes;
        return false;
    }

    const QSize size(handle->width, handle->height);
    NativeTextureCleanup cleanup {
        NativeTextureCleanup::Type::OpenGLTexture,
        glTex,
        reinterpret_cast<void *>(eglImage),
        reinterpret_cast<void *>(eglDisplay),
    };

    texture->setOwnsTexture(false);
    texture->setTextureFromNativeTexture(rhi, glTex, 0, 0, size, {},
                                          QQuickWindowPrivate::TextureFromNativeTextureFlags{});
    texture->setHasAlphaChannel(drmFormatLikelyHasAlpha(dmabuf.format));
    texture->setTextureSize(size);

    if (!texture->rhiTexture()) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan+GL direct texture import failed: QRhiTexture wrapper creation failed"
            << buffer << "size" << size;
        releaseNativeTexture(&cleanup);
        return false;
    }

    *nativeCleanup = cleanup;
    qCDebug(lcWlRenderHelper)
        << "Vulkan+GL direct texture import succeeded"
        << buffer
        << "size" << size
        << "format" << Qt::hex << dmabuf.format << Qt::dec
        << "modifier" << Qt::hex << dmabuf.modifier << Qt::dec
        << "planes" << dmabuf.n_planes
        << "alpha" << texture->hasAlphaChannel();
    return true;
#else
    Q_UNUSED(rhi);
    Q_UNUSED(buffer);
    Q_UNUSED(texture);
    Q_UNUSED(nativeCleanup);
    return false;
#endif
}

void WRenderHelper::releaseImportedVulkanTexture(ImportedVulkanTexture *importedTexture)
{
    if (!importedTexture)
        return;

    delete importedTexture->texture;
    importedTexture->texture = nullptr;
    releaseNativeTexture(&importedTexture->nativeCleanup);
    *importedTexture = {};
}

bool WRenderHelper::importVulkanTextureFromBuffer(QRhi *rhi, qw_buffer *buffer,
                                                  wlr_surface *surface,
                                                  ImportedVulkanTexture *importedTexture)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!rhi || !buffer || !importedTexture)
        return false;

    releaseImportedVulkanTexture(importedTexture);

    if (rhi->backend() != QRhi::Vulkan)
        return false;

    auto *handle = buffer->handle();
    if (!handle || handle->width <= 0 || handle->height <= 0)
        return false;

    const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!handles || !handles->inst || handles->inst->vkInstance() == VK_NULL_HANDLE
        || handles->physDev == VK_NULL_HANDLE || handles->dev == VK_NULL_HANDLE) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import rejected: QRhi Vulkan native handles unavailable"
            << buffer << "size" << handle->width << "x" << handle->height;
        return false;
    }

    wlr_dmabuf_attributes dmabuf;
    if (!buffer->get_dmabuf(&dmabuf)) {
        qCDebug(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import skipped: buffer has no dmabuf"
            << buffer << "size" << handle->width << "x" << handle->height;
        return false;
    }

    bool usedExplicitAcquire = false;
    if (!waitSurfaceExplicitAcquireFence(surface,
                                         "Vulkan RHI dmabuf texture",
                                         &usedExplicitAcquire)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import rejected: explicit acquire wait failed"
            << buffer
            << "size" << QSize(dmabuf.width, dmabuf.height)
            << "format" << drmFormatToName(dmabuf.format)
            << "modifier" << drmModifierToName(dmabuf.modifier)
            << "planes" << dmabuf.n_planes;
        return false;
    }

    if (!usedExplicitAcquire
        && !waitDmabufImplicitFence(buffer, DMA_BUF_SYNC_READ,
                                    "Vulkan RHI dmabuf texture", "implicit acquire")) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import rejected: implicit producer fence wait failed"
            << buffer
            << "size" << QSize(dmabuf.width, dmabuf.height)
            << "format" << drmFormatToName(dmabuf.format)
            << "modifier" << drmModifierToName(dmabuf.modifier)
            << "planes" << dmabuf.n_planes;
        return false;
    }

    auto imported = std::make_unique<VulkanImportedNativeTexture>();
    if (!importDmabufAsVulkanNativeTexture(handles->inst->vkInstance(),
                                           handles->physDev,
                                           handles->dev,
                                           &dmabuf,
                                           imported.get())) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import failed: cannot create sampled VkImage"
            << buffer
            << "size" << QSize(dmabuf.width, dmabuf.height)
            << "format" << drmFormatToName(dmabuf.format)
            << "modifier" << drmModifierToName(dmabuf.modifier)
            << "planes" << dmabuf.n_planes;
        return false;
    }

    if (!acquireVulkanNativeTextureForSampling(rhi, imported.get())) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import failed: cannot acquire image for Qt sampling"
            << buffer
            << "image" << Qt::hex << vulkanHandleToInteger(imported->image) << Qt::dec
            << "size" << imported->size
            << "format" << drmFormatToName(imported->drmFormat)
            << "modifier" << drmModifierToName(imported->drmModifier);
        destroyVulkanImportedNativeTexture(imported.get());
        return false;
    }

    QRhiTexture::Flags textureFlags;
    QRhiTexture::Format textureFormat = QRhiTexture::RGBA8;
    QRhiTexture::Flags formatFlags;
    const auto rhiFormat =
        QSGRhiSupport::instance()->toRhiTextureFormat(imported->format, &formatFlags);
    if (rhiFormat != QRhiTexture::UnknownFormat) {
        textureFormat = rhiFormat;
        textureFlags |= formatFlags;
    }

    std::unique_ptr<QRhiTexture> texture(rhi->newTexture(textureFormat,
                                                         imported->size,
                                                         1,
                                                         textureFlags));
    if (!texture) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import failed: QRhiTexture allocation failed"
            << buffer
            << "image" << Qt::hex << vulkanHandleToInteger(imported->image) << Qt::dec
            << "size" << imported->size
            << "format" << drmFormatToName(imported->drmFormat)
            << "modifier" << drmModifierToName(imported->drmModifier)
            << "viewVkFormat" << imported->format;
        destroyVulkanImportedNativeTexture(imported.get());
        return false;
    }

    if (!texture->createFrom({vulkanHandleToInteger(imported->image), imported->layout})) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI dmabuf texture import failed: QRhiTexture::createFrom rejected native image"
            << buffer
            << "image" << Qt::hex << vulkanHandleToInteger(imported->image) << Qt::dec
            << "size" << imported->size
            << "format" << drmFormatToName(imported->drmFormat)
            << "modifier" << drmModifierToName(imported->drmModifier)
            << "viewVkFormat" << imported->format
            << "layout" << vkImageLayoutName(imported->layout)
            << "rhiFormat" << textureFormat
            << "flags" << textureFlags;
        texture.reset();
        destroyVulkanImportedNativeTexture(imported.get());
        return false;
    }

    VulkanImportedNativeTexture *ownedImport = imported.release();
    importedTexture->texture = texture.release();
    importedTexture->nativeCleanup = {
        NativeTextureCleanup::Type::VulkanTexture,
        vulkanHandleToInteger(ownedImport->image),
        nullptr,
        nullptr,
        ownedImport,
    };
    importedTexture->size = ownedImport->size;
    importedTexture->drmFormat = ownedImport->drmFormat;
    importedTexture->drmModifier = ownedImport->drmModifier;
    importedTexture->hasAlpha = drmFormatLikelyHasAlpha(dmabuf.format)
        && !surfaceOpaqueRegionCoversBuffer(surface, buffer);

    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI dmabuf QRhiTexture import ready"
        << buffer
        << "image" << Qt::hex << vulkanHandleToInteger(ownedImport->image) << Qt::dec
        << "size" << ownedImport->size
        << "format" << drmFormatToName(ownedImport->drmFormat)
        << "modifier" << drmModifierToName(ownedImport->drmModifier)
        << "viewVkFormat" << ownedImport->format
        << "layout" << vkImageLayoutName(ownedImport->layout)
        << "planes" << dmabuf.n_planes
        << "usedExplicitAcquire" << usedExplicitAcquire
        << "usedImplicitAcquire" << !usedExplicitAcquire
        << "rhiFormat" << textureFormat
        << "alpha" << importedTexture->hasAlpha;
    return true;
#else
    Q_UNUSED(rhi);
    Q_UNUSED(buffer);
    Q_UNUSED(surface);
    Q_UNUSED(importedTexture);
    return false;
#endif
}

bool WRenderHelper::acquireImportedVulkanTextureFromBuffer(QRhi *rhi,
                                                           qw_buffer *buffer,
                                                           wlr_surface *surface,
                                                           NativeTextureCleanup *nativeCleanup)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!rhi || !buffer || !nativeCleanup
        || nativeCleanup->type != NativeTextureCleanup::Type::VulkanTexture
        || !nativeCleanup->nativeData) {
        return false;
    }

    wlr_dmabuf_attributes dmabuf = {};
    if (!buffer->get_dmabuf(&dmabuf))
        return false;

    bool usedExplicitAcquire = false;
    if (!waitSurfaceExplicitAcquireFence(surface,
                                         "Vulkan RHI cached dmabuf texture",
                                         &usedExplicitAcquire)) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI cached dmabuf texture acquire rejected: explicit acquire wait failed"
            << buffer
            << "size" << QSize(dmabuf.width, dmabuf.height)
            << "format" << drmFormatToName(dmabuf.format)
            << "modifier" << drmModifierToName(dmabuf.modifier)
            << "planes" << dmabuf.n_planes;
        return false;
    }

    if (!usedExplicitAcquire
        && !waitDmabufImplicitFence(buffer, DMA_BUF_SYNC_READ,
                                    "Vulkan RHI cached dmabuf texture", "implicit acquire")) {
        qCWarning(lcWlRenderHelper)
            << "Vulkan RHI cached dmabuf texture acquire rejected: implicit producer fence wait failed"
            << buffer
            << "size" << QSize(dmabuf.width, dmabuf.height)
            << "format" << drmFormatToName(dmabuf.format)
            << "modifier" << drmModifierToName(dmabuf.modifier)
            << "planes" << dmabuf.n_planes;
        return false;
    }

    auto *imported = static_cast<VulkanImportedNativeTexture *>(nativeCleanup->nativeData);
    if (!imported || !imported->isValid())
        return false;

    imported->layout = VK_IMAGE_LAYOUT_GENERAL;
    if (!acquireVulkanNativeTextureForSampling(rhi, imported))
        return false;

    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI cached dmabuf texture reacquired for Qt sampling"
        << buffer
        << "image" << Qt::hex << vulkanHandleToInteger(imported->image) << Qt::dec
        << "size" << imported->size
        << "format" << drmFormatToName(imported->drmFormat)
        << "modifier" << drmModifierToName(imported->drmModifier)
        << "usedExplicitAcquire" << usedExplicitAcquire
        << "usedImplicitAcquire" << !usedExplicitAcquire;
    return true;
#else
    Q_UNUSED(rhi);
    Q_UNUSED(buffer);
    Q_UNUSED(surface);
    Q_UNUSED(nativeCleanup);
    return false;
#endif
}

bool WRenderHelper::makeVulkanTextureFromBuffer(QRhi *rhi, qw_buffer *buffer,
                                                QSGPlainTexture *texture,
                                                NativeTextureCleanup *nativeCleanup,
                                                wlr_surface *surface)
{
#ifdef ENABLE_VULKAN_RENDER
    if (!rhi || !buffer || !texture || !nativeCleanup)
        return false;

    ImportedVulkanTexture imported;
    if (!importVulkanTextureFromBuffer(rhi, buffer, surface, &imported))
        return false;

    texture->setOwnsTexture(false);
    texture->setTexture(imported.texture);
    texture->setHasAlphaChannel(imported.hasAlpha);
    texture->setTextureSize(imported.size);
    *nativeCleanup = imported.nativeCleanup;
    imported.texture = nullptr;
    imported.nativeCleanup = {};

    qCDebug(lcWlRenderHelper)
        << "Vulkan RHI dmabuf texture import ready"
        << buffer
        << "rhiTexture" << texture->rhiTexture()
        << "size" << texture->textureSize()
        << "format" << drmFormatToName(imported.drmFormat)
        << "modifier" << drmModifierToName(imported.drmModifier)
        << "alpha" << texture->hasAlphaChannel();
    return true;
#else
    Q_UNUSED(rhi);
    Q_UNUSED(buffer);
    Q_UNUSED(texture);
    Q_UNUSED(nativeCleanup);
    return false;
#endif
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
    auto *qbuffer = qw_buffer::from(buffer);

    const auto qformat = static_cast<QRhiTexture::Format>(rhiFormat);
    const auto qflags = QRhiTexture::Flags(rhiFlags);
    std::unique_ptr<QRhiTexture> rhiTexture(rhi->newTexture(qformat, size, 1, qflags));
    NativeTextureCleanup nativeCleanup;

#ifdef ENABLE_VULKAN_RENDER
    const bool importVulkanRenderTarget =
        rhi
        && rhi->backend() == QRhi::Vulkan
        && renderer
        && renderer->handle()
        && renderer->is_vk();
    if (importVulkanRenderTarget) {
        const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
        if (!handles || !handles->inst || handles->inst->vkInstance() == VK_NULL_HANDLE
            || handles->physDev == VK_NULL_HANDLE || handles->dev == VK_NULL_HANDLE) {
            qCCritical(lcWlRenderHelper)
                << "Vulkan RHI newTexture: QRhi Vulkan native handles unavailable";
            qbuffer->drop();
            return {};
        }

        wlr_dmabuf_attributes dmabuf = {};
        if (!qbuffer->get_dmabuf(&dmabuf)) {
            qCCritical(lcWlRenderHelper)
                << "Vulkan RHI newTexture: buffer has no dmabuf";
            qbuffer->drop();
            return {};
        }

        const VkImageUsageFlags imageUsage = vulkanImageUsageForRhiTextureFlags(qflags);
        auto imported = std::make_unique<VulkanImportedRenderTarget>();
        if (!importDmabufAsVulkanRenderTarget(handles->inst->vkInstance(),
                                              handles->physDev,
                                              handles->dev,
                                              &dmabuf,
                                              imageUsage,
                                              false,
                                              imported.get())) {
            qCCritical(lcWlRenderHelper)
                << "Vulkan RHI newTexture: failed to import dmabuf as renderable QRhiTexture"
                << "size" << size
                << "format" << drmFormatToName(dmabuf.format)
                << "modifier" << drmModifierToName(dmabuf.modifier)
                << "usage" << Qt::hex << imageUsage << Qt::dec;
            qbuffer->drop();
            return {};
        }

        if (!rhiTexture->createFrom({vulkanHandleToInteger(imported->image),
                                     imported->layout})) {
            qCCritical(lcWlRenderHelper)
                << "Vulkan RHI newTexture: createFrom imported render target failed"
                << "image" << Qt::hex << vulkanHandleToInteger(imported->image)
                << "layout" << imported->layout << Qt::dec
                << "size" << imported->size
                << "format" << drmFormatToName(imported->drmFormat)
                << "modifier" << drmModifierToName(imported->drmModifier);
            destroyVulkanImportedRenderTarget(imported.get());
            qbuffer->drop();
            return {};
        }

        nativeCleanup = {
            NativeTextureCleanup::Type::VulkanRenderTarget,
            vulkanHandleToInteger(imported->image),
            nullptr,
            nullptr,
            imported.release(),
        };

        rhiTexture->setName("WaylibTexture");
        return {buffer, nullptr, rhiTexture.release(), nativeCleanup};
    }
#endif

    std::unique_ptr<qw_texture> texture(qw_texture::from_buffer(*renderer, *qbuffer));
    if (!texture) {
        qCCritical(lcWlRenderHelper) << "Failed to create qw_texture from buffer";
        qbuffer->drop();
        return {};
    }

    if (texture->is_gles2()) {
        if (rhi->backend() != QRhi::OpenGLES2) {
            qFatal("The current QRhi backend doesn't support creating texture from GLES2 texture");
        }

        wlr_gles2_texture_attribs attribs;
        texture->get_attribs(&attribs);

        if (!rhiTexture->createFrom({attribs.tex, 0})) {
            qCCritical(lcWlRenderHelper, "Failed to create QRhiTexture from GLES2 texture");
            qbuffer->drop();
            return {};
        }
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (texture->is_vk()) {
        // Historical bridge for callers that still combine a Vulkan wlroots
        // renderer with an OpenGL QRhi. treeland itself forces Vulkan QRhi when
        // WLR_RENDERER=vulkan, so normal render-buffer-node allocations are
        // imported above as renderable Vulkan images before creating qw_texture.
        if (rhi->backend() == QRhi::Vulkan) {
            wlr_vk_image_attribs vkAttribs;
            texture->get_image_attribs(&vkAttribs);

            if (!rhiTexture->createFrom({vkimage_cast(vkAttribs.image), vkAttribs.layout})) {
                qCCritical(lcWlRenderHelper, "Failed to create QRhiTexture from Vulkan image");
                qbuffer->drop();
                return {};
            }
        } else if (rhi->backend() == QRhi::OpenGLES2) {
            wlr_dmabuf_attributes dmabuf;
            if (!qbuffer->get_dmabuf(&dmabuf)) {
                qCCritical(lcWlRenderHelper, "Vulkan+GL newTexture: buffer has no dmabuf");
                qbuffer->drop();
                return {};
            }
            EGLDisplay eglDisplay = eglGetCurrentDisplay();
            EGLImage eglImage = EGL_NO_IMAGE;
            GLuint glTex = 0;
            if (!eglImportDmabufToGLTexture(eglDisplay, &dmabuf, &eglImage, &glTex)) {
                qCCritical(lcWlRenderHelper, "Vulkan+GL newTexture: EGL dmabuf import failed");
                qbuffer->drop();
                return {};
            }
            nativeCleanup = {
                NativeTextureCleanup::Type::OpenGLTexture,
                glTex,
                reinterpret_cast<void *>(eglImage),
                reinterpret_cast<void *>(eglDisplay),
            };
            if (!rhiTexture->createFrom({glTex, 0})) {
                qCCritical(lcWlRenderHelper, "Vulkan+GL newTexture: createFrom GL texture failed");
                releaseNativeTexture(&nativeCleanup);
                qbuffer->drop();
                return {};
            }
        } else {
            qFatal("The current QRhi backend doesn't support creating texture from Vulkan image");
        }
    }
#endif
    else if (texture->is_pixman()) {
        qFatal("Creating QRhiTexture from Pixman image is not supported");
    } else {
        qFatal("Unknown texture type");
    }

    rhiTexture->setName("WaylibTexture");

    return {buffer, texture.release(), rhiTexture.release(), nativeCleanup};
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
