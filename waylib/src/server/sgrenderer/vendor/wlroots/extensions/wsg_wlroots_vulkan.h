// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
//
// Extension header for vendored wlroots vulkan renderer.
// Exposes internal texture/command buffer APIs for use by the BAL layer.
// In the vulkan backend, render targets are VkImages wrapped in wlr_vk_texture.

#pragma once

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

#include "render/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

// === Texture handle access (render target) ===

VkImage wsg_wlroots_vulkan_texture_get_image(struct wlr_vk_texture *texture);
uint32_t wsg_wlroots_vulkan_texture_get_mem_count(struct wlr_vk_texture *texture);
VkDeviceMemory wsg_wlroots_vulkan_texture_get_memory(struct wlr_vk_texture *texture,
    uint32_t plane);
bool wsg_wlroots_vulkan_texture_is_dmabuf_imported(struct wlr_vk_texture *texture);
bool wsg_wlroots_vulkan_texture_is_transitioned(struct wlr_vk_texture *texture);
bool wsg_wlroots_vulkan_texture_has_alpha(struct wlr_vk_texture *texture);

// === Command buffer management ===

// Acquires a command buffer from the renderer's pool for BAL recording.
// Returns NULL on failure. The caller is responsible for beginning/ending
// recording and submitting via wsg_wlroots_vulkan_end_command_buffer().
struct wlr_vk_command_buffer *wsg_wlroots_vulkan_acquire_command_buffer(
    struct wlr_vk_renderer *renderer);

// Returns the VkCommandBuffer from a wlr_vk_command_buffer.
VkCommandBuffer wsg_wlroots_vulkan_command_buffer_get_vk(
    struct wlr_vk_command_buffer *cb);

// Ends and submits a command buffer. Returns the timeline point.
uint64_t wsg_wlroots_vulkan_end_command_buffer(
    struct wlr_vk_command_buffer *cb, struct wlr_vk_renderer *renderer);

// Returns the staging command buffer (already in recording state) for
// quick transfer/layout-transition operations.
VkCommandBuffer wsg_wlroots_vulkan_record_stage_cb(struct wlr_vk_renderer *renderer);

// === Device/queue access ===

VkDevice wsg_wlroots_vulkan_renderer_get_device(struct wlr_vk_renderer *renderer);
VkQueue wsg_wlroots_vulkan_renderer_get_queue(struct wlr_vk_renderer *renderer);
VkCommandPool wsg_wlroots_vulkan_renderer_get_command_pool(struct wlr_vk_renderer *renderer);

// === Vulkan sync (0.19.3 API) ===
// These wrap the sync functions that were split in wlroots 0.19.3:
//   vulkan_sync_render_buffer → vulkan_sync_render_pass_release
//                              + vulkan_sync_render_buffer_acquire
//   vulkan_sync_foreign_texture → vulkan_sync_foreign_texture_acquire

bool wsg_wlroots_vulkan_sync_render_pass_release(struct wlr_vk_renderer *renderer,
    struct wlr_vk_render_pass *pass);
bool wsg_wlroots_vulkan_sync_foreign_texture_acquire(struct wlr_vk_texture *texture,
    int sync_file_fds[static WLR_DMABUF_MAX_PLANES]);
bool wsg_wlroots_vulkan_sync_render_buffer_acquire(struct wlr_vk_render_buffer *render_buffer,
    int sync_file_fds[static WLR_DMABUF_MAX_PLANES]);

#ifdef __cplusplus
}
#endif
