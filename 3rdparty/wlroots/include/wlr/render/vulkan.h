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

bool wlr_renderer_is_vk(struct wlr_renderer *wlr_renderer);
bool wlr_texture_is_vk(struct wlr_texture *texture);

void wlr_vk_texture_get_image_attribs(struct wlr_texture *texture,
	struct wlr_vk_image_attribs *attribs);
bool wlr_vk_texture_has_alpha(struct wlr_texture *texture);

/* waylib extensions (not upstream): Qt Quick RHI bridge helpers.
 * Reuse wlroots' own wlr_vk_render_buffer (same path as tinywl/scene),
 * instead of creating a parallel VkImage import of the scanout dmabuf. */
bool waylib_vk_renderer_get_render_buffer_attribs(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, struct wlr_vk_image_attribs *attribs);
bool waylib_vk_renderer_record_render_buffer_acquire(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, VkCommandBuffer cb);
bool waylib_vk_renderer_record_render_buffer_release(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer, VkCommandBuffer cb, VkImageLayout old_layout);
bool waylib_vk_renderer_flush_stage(struct wlr_renderer *renderer);

/* waylib extensions (not upstream): Qt Quick RHI texture-sampling helpers. */
VkQueue waylib_vk_renderer_get_queue(struct wlr_renderer *renderer);
bool waylib_vk_renderer_has_separate_depth_stencil_layouts(
	struct wlr_renderer *renderer);

// Prepare a wlroots-owned Vulkan texture for sampling on the given command
// buffer, transitioning it to SHADER_READ_ONLY_OPTIMAL and transferring queue
// ownership to the graphics queue if it is not already owned. The optional
// attribs out-parameter receives the texture's image attributes.
bool waylib_vk_renderer_prepare_texture_for_sampling(struct wlr_renderer *renderer,
	struct wlr_texture *texture, VkCommandBuffer cb,
	struct wlr_vk_image_attribs *attribs);
bool waylib_vk_renderer_finish_texture_sampling(struct wlr_renderer *renderer,
	struct wlr_texture *texture, VkCommandBuffer cb);

// Collect foreign-texture sync_files while a compositor frame is recorded,
// then submit one semaphore wait with a bridge barrier before the compositor
// command buffer is submitted to the same queue. The barrier extends the wait
// dependency to that later submission. The abort function is idempotent.
bool waylib_vk_renderer_begin_texture_sync_batch(struct wlr_renderer *renderer);
bool waylib_vk_renderer_flush_texture_sync_batch(struct wlr_renderer *renderer);
void waylib_vk_renderer_abort_texture_sync_batch(struct wlr_renderer *renderer);

// Batch the per-texture queue-family-ownership/layout barriers that
// waylib_vk_renderer_prepare_texture_for_sampling() and
// waylib_vk_renderer_finish_texture_sampling() would otherwise emit one at a
// time. Begin a batch (release=false for the pre-draw acquire phase,
// release=true for the post-draw release phase), call prepare/finish for each
// texture, then flush once to record a single vkCmdPipelineBarrier covering
// every accumulated texture. While a batch is active, prepare defers acquire
// barriers and finish defers release barriers; a call belonging to the other
// phase still records immediately so the two phases never mix in one flush.
// abort discards any pending barriers and is idempotent.
bool waylib_vk_renderer_begin_texture_barrier_batch(struct wlr_renderer *renderer,
	bool release);
bool waylib_vk_renderer_flush_texture_barrier_batch(struct wlr_renderer *renderer,
	VkCommandBuffer cb);
void waylib_vk_renderer_abort_texture_barrier_batch(struct wlr_renderer *renderer);

// Enable the GPU-side asynchronous staging-upload path used for shared-memory
// (CPU-rendered) client buffers. It submits the staging copy without blocking
// the caller and chains the upload through the texture-sync bridge before the
// texture is sampled, so it must only be enabled when the consumer (e.g.
// Qt/QRhi) submits its command buffers to the same VkQueue as the renderer and
// flushes that bridge before submission. When disabled (the default), staging
// uploads use a blocking wait.
void waylib_vk_renderer_set_stage_async_enabled(struct wlr_renderer *renderer,
	bool enabled);

#endif
