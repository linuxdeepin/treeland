/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */

#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_VULKAN_H
#define WLR_RENDER_VULKAN_H

#include <stdbool.h>
#include <stdint.h>

#include <vulkan/vulkan_core.h>
#include <wlr/render/wlr_renderer.h>

struct wlr_buffer;

struct wlr_vk_image_attribs {
	VkImage image;
	VkImageLayout layout;
	VkFormat format;
	VkImageUsageFlags usage;
};

struct wlr_renderer *wlr_vk_renderer_create_with_drm_fd(int drm_fd);

VkInstance wlr_vk_renderer_get_instance(struct wlr_renderer *renderer);
VkPhysicalDevice wlr_vk_renderer_get_physical_device(struct wlr_renderer *renderer);
VkDevice wlr_vk_renderer_get_device(struct wlr_renderer *renderer);
uint32_t wlr_vk_renderer_get_queue_family(struct wlr_renderer *renderer);
VkQueue wlr_vk_renderer_get_queue(struct wlr_renderer *renderer);
bool wlr_vk_renderer_has_separate_depth_stencil_layouts(
	struct wlr_renderer *renderer);

bool wlr_renderer_is_vk(struct wlr_renderer *wlr_renderer);
bool wlr_texture_is_vk(struct wlr_texture *texture);

void wlr_vk_texture_get_image_attribs(struct wlr_texture *texture,
	struct wlr_vk_image_attribs *attribs);
bool wlr_vk_texture_has_alpha(struct wlr_texture *texture);

bool wlr_vk_renderer_prepare_texture_for_sampling(struct wlr_renderer *renderer,
	struct wlr_texture *texture, VkCommandBuffer cb,
	struct wlr_vk_image_attribs *attribs);
bool wlr_vk_renderer_finish_texture_sampling(struct wlr_renderer *renderer,
	struct wlr_texture *texture, VkCommandBuffer cb);

// Collect foreign-texture sync_files while a compositor frame is recorded,
// then submit one semaphore-only wait before the compositor command buffer is
// submitted to the same queue. The abort function is idempotent.
bool wlr_vk_renderer_begin_texture_sync_batch(struct wlr_renderer *renderer);
bool wlr_vk_renderer_flush_texture_sync_batch(struct wlr_renderer *renderer);
void wlr_vk_renderer_abort_texture_sync_batch(struct wlr_renderer *renderer);

// Batch the per-texture queue-family-ownership/layout barriers that
// wlr_vk_renderer_prepare_texture_for_sampling() and
// wlr_vk_renderer_finish_texture_sampling() would otherwise emit one at a
// time. Begin a batch (release=false for the pre-draw acquire phase,
// release=true for the post-draw release phase), call prepare/finish for each
// texture, then flush once to record a single vkCmdPipelineBarrier covering
// every accumulated texture. While a batch is active, prepare defers acquire
// barriers and finish defers release barriers; a call belonging to the other
// phase still records immediately so the two phases never mix in one flush.
// abort discards any pending barriers and is idempotent.
bool wlr_vk_renderer_begin_texture_barrier_batch(struct wlr_renderer *renderer,
	bool release);
bool wlr_vk_renderer_flush_texture_barrier_batch(struct wlr_renderer *renderer,
	VkCommandBuffer cb);
void wlr_vk_renderer_abort_texture_barrier_batch(struct wlr_renderer *renderer);

bool wlr_vk_renderer_get_render_buffer_attribs(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, struct wlr_vk_image_attribs *attribs);
bool wlr_vk_renderer_record_render_buffer_acquire(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, VkCommandBuffer cb);
bool wlr_vk_renderer_record_render_buffer_release(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, VkCommandBuffer cb, VkImageLayout old_layout);

/* Compatibility names used by the mainline Qt Quick bridge. New waylib code
 * should call the wlr_vk_renderer_* variants above directly. */
bool waylib_vk_renderer_get_render_buffer_attribs(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, struct wlr_vk_image_attribs *attribs);
bool waylib_vk_renderer_record_render_buffer_acquire(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, VkCommandBuffer cb);
bool waylib_vk_renderer_record_render_buffer_release(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, VkCommandBuffer cb, VkImageLayout old_layout);
bool waylib_vk_renderer_flush_stage(struct wlr_renderer *renderer);
#endif
