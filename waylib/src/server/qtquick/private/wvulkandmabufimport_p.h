// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

struct wlr_dmabuf_attributes;

#ifdef ENABLE_VULKAN_RENDER
#include <vulkan/vulkan.h>

#include <wlr/render/dmabuf.h>
#endif

WAYLIB_SERVER_BEGIN_NAMESPACE

#ifdef ENABLE_VULKAN_RENDER

// Holds a Vulkan image imported from a dmabuf so it can be used as a render
// target (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT). Unlike the image produced by
// wlr_texture_from_buffer (which is sampled-only), this image is suitable for
// QRhiTextureRenderTarget. The caller owns the resources and must release them
// with vulkanReleaseDmabufImage() before the VkDevice is destroyed.
struct VkDmabufImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memories[WLR_DMABUF_MAX_PLANES] = {};
    uint32_t memoryCount = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkDevice device = VK_NULL_HANDLE;

    bool isNull() const { return image == VK_NULL_HANDLE; }
};

// Imports a dmabuf as a VkImage with render-target usage. Returns a non-null
// image on success; on failure image is VK_NULL_HANDLE and no resources are
// leaked. Implementation is adapted from wlroots 0.20 vulkan_import_dmabuf
// (render/vulkan/texture.c) but uses only the public Vulkan and wlroots API,
// so it does not depend on wlroots' private renderer structs.
VkDmabufImage vulkanImportDmabufForRender(VkPhysicalDevice physicalDevice,
                                          VkDevice device,
                                          const wlr_dmabuf_attributes *attribs);

// Releases the image and memory held by an imported dmabuf image. Safe to call
// on a default-constructed (null) instance.
void vulkanReleaseDmabufImage(VkDmabufImage &import);
#endif // ENABLE_VULKAN_RENDER

WAYLIB_SERVER_END_NAMESPACE
