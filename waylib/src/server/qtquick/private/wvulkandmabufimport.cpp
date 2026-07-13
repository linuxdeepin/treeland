// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// This file adapts wlroots 0.20's internal vulkan_import_dmabuf() and its
// helpers so waylib can import a dmabuf as a VkImage with render-target usage
// (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT). The wlroots function lives in a
// private, uninstalled header (include/render/vulkan.h) and reaches into
// private renderer structs (wlr_vk_device::format_props, ::api). To avoid
// linking against those internals, the queries below use only the public
// Vulkan and public wlroots renderer API. The logic mirrors wlroots 0.20
// (render/vulkan/texture.c, pixel_format.c, util.c) so it can be updated by
// diffing against upstream.

#include "wvulkandmabufimport_p.h"

#ifdef ENABLE_VULKAN_RENDER

#include "wayliblogging.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <drm_fourcc.h>

#include <QLoggingCategory>

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wmissing-field-initializers")
QT_WARNING_DISABLE_CLANG("-Wmissing-field-initializers")
QT_WARNING_DISABLE_CLANG("-Wmissing-designated-field-initializers")

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

// DRM <-> VkFormat mapping for render-target import (adapted from wlroots
// 0.20 render/vulkan/pixel_format.c).
struct WlrVkFormat {
    uint32_t drm;
    VkFormat vk;
    VkFormat vk_srgb; // sRGB variant, or 0 if nonexistent
};

// Copy from wlroots 0.20 render/vulkan/pixel_format.c: the DRM <-> VkFormat
// mapping table. Only the formats wlroots knows about are listed; the GPU may
// support a subset.
const WlrVkFormat kVkFormats[] = {
    // Vulkan non-packed 8-bits-per-channel formats have an inverted channel
    // order compared to the DRM formats, because DRM format channel order
    // is little-endian while Vulkan format channel order is in memory byte
    // order.
    {.drm = DRM_FORMAT_R8,              .vk = VK_FORMAT_R8_UNORM,  .vk_srgb = VK_FORMAT_R8_SRGB},
#if defined(DRM_FORMAT_R16F)
    {.drm = DRM_FORMAT_R16F,            .vk = VK_FORMAT_R16_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
#if defined(DRM_FORMAT_R32F)
    {.drm = DRM_FORMAT_R32F,            .vk = VK_FORMAT_R32_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
    {.drm = DRM_FORMAT_GR88,            .vk = VK_FORMAT_R8G8_UNORM, .vk_srgb = VK_FORMAT_R8G8_SRGB},
#if defined(DRM_FORMAT_GR1616F)
    {.drm = DRM_FORMAT_GR1616F,         .vk = VK_FORMAT_R16G16_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
#if defined(DRM_FORMAT_GR3232F)
    {.drm = DRM_FORMAT_GR3232F,         .vk = VK_FORMAT_R32G32_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
    {.drm = DRM_FORMAT_RGB888,          .vk = VK_FORMAT_B8G8R8_UNORM, .vk_srgb = VK_FORMAT_B8G8R8_SRGB},
    {.drm = DRM_FORMAT_BGR888,          .vk = VK_FORMAT_R8G8B8_UNORM, .vk_srgb = VK_FORMAT_R8G8B8_SRGB},
    {.drm = DRM_FORMAT_XRGB8888,        .vk = VK_FORMAT_B8G8R8A8_UNORM, .vk_srgb = VK_FORMAT_B8G8R8A8_SRGB},
    {.drm = DRM_FORMAT_XBGR8888,        .vk = VK_FORMAT_R8G8B8A8_UNORM, .vk_srgb = VK_FORMAT_R8G8B8A8_SRGB},
    // The Vulkan _SRGB formats correspond to unpremultiplied alpha, but
    // the Wayland protocol specifies premultiplied alpha on electrical values
    {.drm = DRM_FORMAT_ARGB8888,        .vk = VK_FORMAT_B8G8R8A8_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_ABGR8888,        .vk = VK_FORMAT_R8G8B8A8_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    // Vulkan packed formats have the same channel order as DRM formats on
    // little endian systems.
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    {.drm = DRM_FORMAT_RGBA4444,        .vk = VK_FORMAT_R4G4B4A4_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_RGBX4444,        .vk = VK_FORMAT_R4G4B4A4_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_BGRA4444,        .vk = VK_FORMAT_B4G4R4A4_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_BGRX4444,        .vk = VK_FORMAT_B4G4R4A4_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_RGB565,          .vk = VK_FORMAT_R5G6B5_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_BGR565,          .vk = VK_FORMAT_B5G6R5_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_RGBA5551,        .vk = VK_FORMAT_R5G5B5A1_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_RGBX5551,        .vk = VK_FORMAT_R5G5B5A1_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_BGRA5551,        .vk = VK_FORMAT_B5G5R5A1_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_BGRX5551,        .vk = VK_FORMAT_B5G5R5A1_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_ARGB1555,        .vk = VK_FORMAT_A1R5G5B5_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_XRGB1555,        .vk = VK_FORMAT_A1R5G5B5_UNORM_PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_ARGB2101010,     .vk = VK_FORMAT_A2R10G10B10_UNORM_PACK32, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_XRGB2101010,     .vk = VK_FORMAT_A2R10G10B10_UNORM_PACK32, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_ABGR2101010,     .vk = VK_FORMAT_A2B10G10R10_UNORM_PACK32, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_XBGR2101010,     .vk = VK_FORMAT_A2B10G10R10_UNORM_PACK32, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif

    // Vulkan 16-bits-per-channel formats have an inverted channel order
    // compared to DRM formats, just like the 8-bits-per-channel ones.
    // On little endian systems the memory representation of each channel
    // matches the DRM formats'.
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
#if defined(DRM_FORMAT_BGR161616)
    {.drm = DRM_FORMAT_BGR161616,       .vk = VK_FORMAT_R16G16B16_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
#if defined(DRM_FORMAT_BGR161616F)
    {.drm = DRM_FORMAT_BGR161616F,      .vk = VK_FORMAT_R16G16B16_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
    {.drm = DRM_FORMAT_ABGR16161616,    .vk = VK_FORMAT_R16G16B16A16_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_XBGR16161616,    .vk = VK_FORMAT_R16G16B16A16_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_ABGR16161616F,   .vk = VK_FORMAT_R16G16B16A16_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_XBGR16161616F,   .vk = VK_FORMAT_R16G16B16A16_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#if defined(DRM_FORMAT_BGR323232F)
    {.drm = DRM_FORMAT_BGR323232F,      .vk = VK_FORMAT_R32G32B32_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
#if defined(DRM_FORMAT_ABGR32323232F)
    {.drm = DRM_FORMAT_ABGR32323232F,   .vk = VK_FORMAT_R32G32B32A32_SFLOAT, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
#endif

    // YCbCr formats
    // R -> V, G -> Y, B -> U
    // 420 -> 2x2 subsampled, 422 -> 2x1 subsampled, 444 -> non-subsampled
    {.drm = DRM_FORMAT_UYVY,            .vk = VK_FORMAT_B8G8R8G8_422_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_YUYV,            .vk = VK_FORMAT_G8B8G8R8_422_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_NV12,            .vk = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_NV16,            .vk = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_YUV420,          .vk = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_YUV422,          .vk = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_YUV444,          .vk = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    // 3PACK16 formats split the memory in three 16-bit words, so they have an
    // inverted channel order compared to DRM formats.
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    {.drm = DRM_FORMAT_P010,            .vk = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_P210,            .vk = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_P012,            .vk = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_P016,            .vk = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM, .vk_srgb = VK_FORMAT_UNDEFINED},
    {.drm = DRM_FORMAT_Q410,            .vk = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, .vk_srgb = VK_FORMAT_UNDEFINED},
#endif
    // TODO: add DRM_FORMAT_NV24/VK_FORMAT_G8_B8R8_2PLANE_444_UNORM (requires
    // Vulkan 1.3 or VK_EXT_ycbcr_2plane_444_formats)
};

const WlrVkFormat *vkFormatFromDrm(uint32_t drm)
{
    for (const WlrVkFormat &f : kVkFormats) {
        if (f.drm == drm)
            return &f;
    }
    return nullptr;
}

constexpr bool isYCbCr(uint32_t drm)
{
    switch (drm) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV24:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_NV42:
    case DRM_FORMAT_P010:
    case DRM_FORMAT_P012:
    case DRM_FORMAT_P016:
    case DRM_FORMAT_P030:
    case DRM_FORMAT_P210:
    case DRM_FORMAT_Q401:
    case DRM_FORMAT_Q410:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VUY101010:
    case DRM_FORMAT_XYUV8888:
    case DRM_FORMAT_Y210:
    case DRM_FORMAT_Y212:
    case DRM_FORMAT_Y216:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
        return true;
    default:
        return false;
    }
}

// Copy from wlroots 0.20 render/vulkan/texture.c: a dmabuf is disjoint when
// its planes live in different dmabuf fds (each plane needs its own
// VkDeviceMemory binding in that case).
bool isDmabufDisjoint(const wlr_dmabuf_attributes *attribs)
{
    if (attribs->n_planes == 1)
        return false;

    struct stat firstStat;
    if (fstat(attribs->fd[0], &firstStat) != 0) {
        qCWarning(lcWlRenderHelper, "isDmabufDisjoint: fstat failed");
        return true;
    }

    for (int i = 1; i < attribs->n_planes; i++) {
        struct stat planeStat;
        if (fstat(attribs->fd[i], &planeStat) != 0) {
            qCWarning(lcWlRenderHelper, "isDmabufDisjoint: fstat failed");
            return true;
        }
        if (firstStat.st_ino != planeStat.st_ino)
            return true;
    }

    return false;
}

VkImageAspectFlagBits memPlaneAspect(unsigned i)
{
    switch (i) {
    case 0: return VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
    case 1: return VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;
    case 2: return VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT;
    case 3: return VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT;
    default: Q_UNREACHABLE(); break;
    }
}

// Copy from wlroots 0.20 render/vulkan/util.c: find a memory type that
// satisfies req_bits and the requested property flags.
int findMemType(VkPhysicalDevice phdev, VkMemoryPropertyFlags flags, uint32_t reqBits)
{
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(phdev, &props);

    for (unsigned i = 0; i < props.memoryTypeCount; ++i) {
        if (reqBits & (1u << i)) {
            if ((props.memoryTypes[i].propertyFlags & flags) == flags)
                return static_cast<int>(i);
        }
    }

    return -1;
}

// Render-target usage and the format features wlroots requires for it.
constexpr VkImageUsageFlags kRenderUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
constexpr VkFormatFeatureFlags kRenderFeatures =
    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

// Adapted from wlroots 0.20 render/vulkan/pixel_format.c
// (query_modifier_usage_support): verify that a specific modifier can host a
// render-target image importable from a dmabuf, and report its max extent.
bool queryModifierRenderSupport(VkPhysicalDevice phdev, VkFormat vkFormat,
                                uint64_t modifier, VkExtent2D *outMaxExtent)
{
    VkFormat viewFormats[1] = {vkFormat};
    VkImageFormatListCreateInfoKHR listInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
        .viewFormatCount = 1,
        .pViewFormats = viewFormats,
    };
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modInfo = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .pNext = &listInfo,
        .drmFormatModifier = modifier,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkPhysicalDeviceExternalImageFormatInfo extInfo = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .pNext = &modInfo,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceImageFormatInfo2 fmtInfo = {};
    fmtInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    fmtInfo.pNext = &extInfo;
    fmtInfo.type = VK_IMAGE_TYPE_2D;
    fmtInfo.format = vkFormat;
    fmtInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    fmtInfo.usage = kRenderUsage;
    fmtInfo.flags = 0;
    VkExternalImageFormatProperties extProps = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 imgProps = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &extProps,
    };

    VkResult res = vkGetPhysicalDeviceImageFormatProperties2(phdev, &fmtInfo, &imgProps);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper,
                  "vkGetPhysicalDeviceImageFormatProperties2 failed for render import (res=%d)", res);
        return false;
    }

    if (!(extProps.externalMemoryProperties.externalMemoryFeatures
          & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) {
        qCWarning(lcWlRenderHelper, "dmabuf modifier 0x%016llx is not importable",
                  static_cast<unsigned long long>(modifier));
        return false;
    }

    outMaxExtent->width = imgProps.imageFormatProperties.maxExtent.width;
    outMaxExtent->height = imgProps.imageFormatProperties.maxExtent.height;
    return true;
}

// Combined query for the modifier matching the dmabuf: mirrors wlroots 0.20
// vulkan_format_props_from_drm + vulkan_format_props_find_modifier +
// query_modifier_support, but only for the single modifier we need. Fills the
// modifier plane count, tiling features and max extent.
struct ModifierQueryResult {
    uint32_t planeCount = 0;
    VkFormatFeatureFlags tilingFeatures = 0;
    VkExtent2D maxExtent = {};
    bool supported = false;
};

ModifierQueryResult findRenderModifier(VkPhysicalDevice phdev, VkFormat vkFormat,
                                       uint64_t modifier)
{
    ModifierQueryResult result;

    VkDrmFormatModifierPropertiesListEXT modList = {
        .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 fmtProps = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &modList,
    };
    vkGetPhysicalDeviceFormatProperties2(phdev, vkFormat, &fmtProps);

    if (modList.drmFormatModifierCount == 0) {
        qCWarning(lcWlRenderHelper, "no drm format modifiers for vk format %d", vkFormat);
        return result;
    }

    QList<VkDrmFormatModifierPropertiesEXT> mods(modList.drmFormatModifierCount);
    modList.pDrmFormatModifierProperties = mods.data();
    vkGetPhysicalDeviceFormatProperties2(phdev, vkFormat, &fmtProps);

    for (uint32_t i = 0; i < modList.drmFormatModifierCount; ++i) {
        const VkDrmFormatModifierPropertiesEXT &m = modList.pDrmFormatModifierProperties[i];
        if (m.drmFormatModifier != modifier)
            continue;

        if ((m.drmFormatModifierTilingFeatures & kRenderFeatures) != kRenderFeatures) {
            qCWarning(lcWlRenderHelper,
                      "modifier 0x%016llx lacks render features for vk format %d",
                      static_cast<unsigned long long>(modifier), vkFormat);
            return result;
        }

        VkExtent2D maxExtent = {};
        if (!queryModifierRenderSupport(phdev, vkFormat, modifier, &maxExtent))
            return result;

        result.planeCount = m.drmFormatModifierPlaneCount;
        result.tilingFeatures = m.drmFormatModifierTilingFeatures;
        result.maxExtent = maxExtent;
        result.supported = true;
        return result;
    }

    qCWarning(lcWlRenderHelper, "modifier 0x%016llx not advertised for vk format %d",
              static_cast<unsigned long long>(modifier), vkFormat);
    return result;
}

} // namespace

VkDmabufImage vulkanImportDmabufForRender(VkPhysicalDevice physicalDevice, VkDevice device,
                                          const wlr_dmabuf_attributes *attribs)
{
    VkDmabufImage result;
    result.device = device;

    const WlrVkFormat *fmt = vkFormatFromDrm(attribs->format);
    if (!fmt) {
        qCWarning(lcWlRenderHelper, "Unsupported pixel format 0x%08X for vulkan render import",
                  attribs->format);
        return result;
    }

    if (isYCbCr(attribs->format)) {
        qCWarning(lcWlRenderHelper, "YCbCr format 0x%08X cannot be used as a render target",
                  attribs->format);
        return result;
    }

    const uint32_t planeCount = attribs->n_planes;
    if (planeCount >= WLR_DMABUF_MAX_PLANES) {
        qCWarning(lcWlRenderHelper, "Too many dmabuf planes (%u)", planeCount);
        return result;
    }

    ModifierQueryResult mod = findRenderModifier(physicalDevice, fmt->vk, attribs->modifier);
    if (!mod.supported)
        return result;

    if (static_cast<uint32_t>(attribs->width) > mod.maxExtent.width
        || static_cast<uint32_t>(attribs->height) > mod.maxExtent.height) {
        qCWarning(lcWlRenderHelper,
                  "dmabuf too large to import (%dx%d > %ux%u)",
                  attribs->width, attribs->height, mod.maxExtent.width, mod.maxExtent.height);
        return result;
    }

    if (mod.planeCount != planeCount) {
        qCWarning(lcWlRenderHelper, "Plane count mismatch (dmabuf %u, format %u)",
                  planeCount, mod.planeCount);
        return result;
    }

    const bool disjoint = isDmabufDisjoint(attribs);
    if (disjoint && !(mod.tilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT)) {
        qCWarning(lcWlRenderHelper, "Format/modifier does not support disjoint images");
        return result;
    }

    const VkExternalMemoryHandleTypeFlagBits handleType =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo imgInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = fmt->vk,
        .extent = {static_cast<uint32_t>(attribs->width), static_cast<uint32_t>(attribs->height), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = kRenderUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (disjoint)
        imgInfo.flags = VK_IMAGE_CREATE_DISJOINT_BIT;

    VkExternalMemoryImageCreateInfo extImg = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = handleType,
    };
    imgInfo.pNext = &extImg;

    VkSubresourceLayout planeLayouts[WLR_DMABUF_MAX_PLANES] = {};
    imgInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    for (unsigned i = 0; i < planeCount; ++i) {
        planeLayouts[i].offset = attribs->offset[i];
        planeLayouts[i].rowPitch = attribs->stride[i];
        planeLayouts[i].size = 0;
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT modInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
        .drmFormatModifier = attribs->modifier,
        .drmFormatModifierPlaneCount = planeCount,
        .pPlaneLayouts = planeLayouts,
    };
    extImg.pNext = &modInfo;

    VkImage image = VK_NULL_HANDLE;
    VkResult res = vkCreateImage(device, &imgInfo, nullptr, &image);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper, "vkCreateImage failed (res=%d)", res);
        return result;
    }

    // vkGetMemoryFdPropertiesKHR is an extension function wlroots loads into
    // its private device struct; load it ourselves via the device command table.
    PFN_vkGetMemoryFdPropertiesKHR getMemoryFdProperties =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
    if (!getMemoryFdProperties) {
        qCWarning(lcWlRenderHelper, "vkGetMemoryFdPropertiesKHR not available");
        vkDestroyImage(device, image, nullptr);
        return result;
    }

    const unsigned memCount = disjoint ? planeCount : 1u;
    VkBindImageMemoryInfo bindInfo[WLR_DMABUF_MAX_PLANES] = {};
    VkBindImagePlaneMemoryInfo planeInfo[WLR_DMABUF_MAX_PLANES] = {};

    for (unsigned i = 0; i < memCount; ++i) {
        VkMemoryFdPropertiesKHR fdProps = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
        };
        res = getMemoryFdProperties(device, handleType, attribs->fd[i], &fdProps);
        if (res != VK_SUCCESS) {
            qCWarning(lcWlRenderHelper, "vkGetMemoryFdPropertiesKHR failed (res=%d)", res);
            goto error;
        }

        VkImageMemoryRequirementsInfo2 memReqInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .image = image,
        };

        VkImagePlaneMemoryRequirementsInfo planeReq;
        if (disjoint) {
            planeReq = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
                .planeAspect = memPlaneAspect(i),
            };
            memReqInfo.pNext = &planeReq;
        }

        VkMemoryRequirements2 memReq = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        };
        vkGetImageMemoryRequirements2(device, &memReqInfo, &memReq);

        int memType = findMemType(physicalDevice, 0,
                                  memReq.memoryRequirements.memoryTypeBits & fdProps.memoryTypeBits);
        if (memType < 0) {
            qCWarning(lcWlRenderHelper, "no valid memory type index for plane %u", i);
            goto error;
        }

        // Importing a dmabuf fd transfers ownership of the fd to Vulkan, so
        // duplicate it: the attribs still belong to the caller and Vulkan will
        // close the duplicate on vkFreeMemory.
        int dupFd = fcntl(attribs->fd[i], F_DUPFD_CLOEXEC, 0);
        if (dupFd < 0) {
            qCWarning(lcWlRenderHelper, "fcntl(F_DUPFD_CLOEXEC) failed for plane %u", i);
            goto error;
        }

        VkMemoryAllocateInfo memAlloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReq.memoryRequirements.size,
            .memoryTypeIndex = static_cast<uint32_t>(memType),
        };
        VkImportMemoryFdInfoKHR importInfo = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
            .handleType = handleType,
            .fd = dupFd,
        };
        memAlloc.pNext = &importInfo;
        VkMemoryDedicatedAllocateInfo dedicated = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .image = image,
        };
        importInfo.pNext = &dedicated;

        res = vkAllocateMemory(device, &memAlloc, nullptr, &result.memories[i]);
        if (res != VK_SUCCESS) {
            close(dupFd);
            qCWarning(lcWlRenderHelper, "vkAllocateMemory failed for plane %u (res=%d)", i, res);
            goto error;
        }

        ++result.memoryCount;

        bindInfo[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
        bindInfo[i].image = image;
        bindInfo[i].memory = result.memories[i];
        bindInfo[i].memoryOffset = 0;

        if (disjoint) {
            planeInfo[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
            planeInfo[i].planeAspect = planeReq.planeAspect;
            bindInfo[i].pNext = &planeInfo[i];
        }
    }

    res = vkBindImageMemory2(device, memCount, bindInfo);
    if (res != VK_SUCCESS) {
        qCWarning(lcWlRenderHelper, "vkBindImageMemory2 failed (res=%d)", res);
        goto error;
    }

    result.image = image;
    result.format = fmt->vk;
    return result;

error:
    vkDestroyImage(device, image, nullptr);
    for (uint32_t i = 0; i < result.memoryCount; ++i) {
        vkFreeMemory(device, result.memories[i], nullptr);
        result.memories[i] = VK_NULL_HANDLE;
    }
    result.memoryCount = 0;
    return result;
}

void vulkanReleaseDmabufImage(VkDmabufImage &import)
{
    if (import.image != VK_NULL_HANDLE) {
        vkDestroyImage(import.device, import.image, nullptr);
        import.image = VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < import.memoryCount; ++i) {
        if (import.memories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(import.device, import.memories[i], nullptr);
            import.memories[i] = VK_NULL_HANDLE;
        }
    }
    import.memoryCount = 0;
}

WAYLIB_SERVER_END_NAMESPACE

QT_WARNING_POP
#endif // ENABLE_VULKAN_RENDER
