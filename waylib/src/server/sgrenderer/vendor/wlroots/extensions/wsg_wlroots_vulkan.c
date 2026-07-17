// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension implementations for vendored wlroots vulkan renderer.
// These thin wrappers expose internal struct fields for BAL vulkan backend use.

#include "wsg_wlroots_vulkan.h"

VkImage wsg_wlroots_vulkan_texture_get_image(struct wlr_vk_texture *texture) {
    return texture->image;
}

uint32_t wsg_wlroots_vulkan_texture_get_mem_count(struct wlr_vk_texture *texture) {
    return texture->mem_count;
}

VkDeviceMemory wsg_wlroots_vulkan_texture_get_memory(struct wlr_vk_texture *texture,
    uint32_t plane) {
    if (plane >= texture->mem_count) {
        return VK_NULL_HANDLE;
    }
    return texture->memories[plane];
}

bool wsg_wlroots_vulkan_texture_is_dmabuf_imported(struct wlr_vk_texture *texture) {
    return texture->dmabuf_imported;
}

bool wsg_wlroots_vulkan_texture_is_transitioned(struct wlr_vk_texture *texture) {
    return texture->transitioned;
}

bool wsg_wlroots_vulkan_texture_has_alpha(struct wlr_vk_texture *texture) {
    return texture->has_alpha;
}

struct wlr_vk_command_buffer *wsg_wlroots_vulkan_acquire_command_buffer(
    struct wlr_vk_renderer *renderer) {
    return vulkan_acquire_command_buffer(renderer);
}

VkCommandBuffer wsg_wlroots_vulkan_command_buffer_get_vk(
    struct wlr_vk_command_buffer *cb) {
    return cb->vk;
}

uint64_t wsg_wlroots_vulkan_end_command_buffer(
    struct wlr_vk_command_buffer *cb, struct wlr_vk_renderer *renderer) {
    return vulkan_end_command_buffer(cb, renderer);
}

VkCommandBuffer wsg_wlroots_vulkan_record_stage_cb(struct wlr_vk_renderer *renderer) {
    return vulkan_record_stage_cb(renderer);
}

VkDevice wsg_wlroots_vulkan_renderer_get_device(struct wlr_vk_renderer *renderer) {
    return renderer->dev->dev;
}

VkQueue wsg_wlroots_vulkan_renderer_get_queue(struct wlr_vk_renderer *renderer) {
    return renderer->dev->queue;
}

VkCommandPool wsg_wlroots_vulkan_renderer_get_command_pool(struct wlr_vk_renderer *renderer) {
    return renderer->command_pool;
}

bool wsg_wlroots_vulkan_sync_render_pass_release(struct wlr_vk_renderer *renderer,
    struct wlr_vk_render_pass *pass) {
    return vulkan_sync_render_pass_release(renderer, pass);
}

bool wsg_wlroots_vulkan_sync_foreign_texture_acquire(struct wlr_vk_texture *texture,
    int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
    return vulkan_sync_foreign_texture_acquire(texture, sync_file_fds);
}

bool wsg_wlroots_vulkan_sync_render_buffer_acquire(struct wlr_vk_render_buffer *render_buffer,
    int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
    return vulkan_sync_render_buffer_acquire(render_buffer, sync_file_fds);
}
