// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

#ifdef WLR_HAVE_VULKAN_RENDERER

#include <qwbuffer.h>
#include <qwrenderer.h>
#include <qwtexture.h>

extern "C" {
#include <wlr/render/vulkan.h>
}

QW_BEGIN_NAMESPACE

namespace qw_vulkan {

inline bool isRenderer(qw_renderer *renderer)
{
    return wlr_renderer_is_vk(renderer->handle());
}

inline bool isTexture(qw_texture *texture)
{
    return wlr_texture_is_vk(texture->handle());
}

inline VkInstance rendererInstance(qw_renderer *renderer)
{
    return wlr_vk_renderer_get_instance(renderer->handle());
}

inline VkPhysicalDevice rendererPhysicalDevice(qw_renderer *renderer)
{
    return wlr_vk_renderer_get_physical_device(renderer->handle());
}

inline VkDevice rendererDevice(qw_renderer *renderer)
{
    return wlr_vk_renderer_get_device(renderer->handle());
}

inline uint32_t rendererQueueFamily(qw_renderer *renderer)
{
    return wlr_vk_renderer_get_queue_family(renderer->handle());
}

inline void textureImageAttribs(qw_texture *texture, wlr_vk_image_attribs *attribs)
{
    wlr_vk_texture_get_image_attribs(texture->handle(), attribs);
}

inline bool textureHasAlpha(qw_texture *texture)
{
    return wlr_vk_texture_has_alpha(texture->handle());
}

inline bool prepareTextureForSampling(qw_renderer *renderer, qw_texture *texture,
                                      VkCommandBuffer commandBuffer,
                                      wlr_vk_image_attribs *attribs)
{
    return wlr_vk_renderer_prepare_texture_for_sampling(renderer->handle(), texture->handle(),
                                                         commandBuffer, attribs);
}

inline bool finishTextureSampling(qw_renderer *renderer, qw_texture *texture,
                                  VkCommandBuffer commandBuffer)
{
    return wlr_vk_renderer_finish_texture_sampling(renderer->handle(), texture->handle(),
                                                    commandBuffer);
}

inline bool renderBufferAttribs(qw_renderer *renderer, wlr_buffer *buffer,
                                wlr_vk_image_attribs *attribs)
{
    return wlr_vk_renderer_get_render_buffer_attribs(renderer->handle(), buffer, attribs);
}

inline bool renderBufferAttribs(qw_renderer *renderer, qw_buffer *buffer,
                                wlr_vk_image_attribs *attribs)
{
    return renderBufferAttribs(renderer, buffer->handle(), attribs);
}

inline bool recordRenderBufferAcquire(qw_renderer *renderer, wlr_buffer *buffer,
                                      VkCommandBuffer commandBuffer)
{
    return wlr_vk_renderer_record_render_buffer_acquire(renderer->handle(), buffer,
                                                         commandBuffer);
}

inline bool recordRenderBufferAcquire(qw_renderer *renderer, qw_buffer *buffer,
                                      VkCommandBuffer commandBuffer)
{
    return recordRenderBufferAcquire(renderer, buffer->handle(), commandBuffer);
}

inline bool recordRenderBufferRelease(qw_renderer *renderer, wlr_buffer *buffer,
                                      VkCommandBuffer commandBuffer,
                                      VkImageLayout oldLayout)
{
    return wlr_vk_renderer_record_render_buffer_release(renderer->handle(), buffer,
                                                         commandBuffer, oldLayout);
}

inline bool recordRenderBufferRelease(qw_renderer *renderer, qw_buffer *buffer,
                                      VkCommandBuffer commandBuffer,
                                      VkImageLayout oldLayout)
{
    return recordRenderBufferRelease(renderer, buffer->handle(), commandBuffer, oldLayout);
}

} // namespace qw_vulkan

QW_END_NAMESPACE

#endif // WLR_HAVE_VULKAN_RENDERER
