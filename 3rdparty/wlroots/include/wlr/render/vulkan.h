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
// sampled_buffer is the wlr_buffer the caller is sampling the texture as (may
// be NULL when the caller cannot provide it): it identifies client textures so
// a repeated acquire of an unchanged client buffer can skip the producer fence
// wait (see waylib_vk_renderer_mark_buffer_content_dirty()).
bool waylib_vk_renderer_prepare_texture_for_sampling(struct wlr_renderer *renderer,
	struct wlr_texture *texture, struct wlr_buffer *sampled_buffer,
	VkCommandBuffer cb, struct wlr_vk_image_attribs *attribs);
bool waylib_vk_renderer_finish_texture_sampling(struct wlr_renderer *renderer,
	struct wlr_texture *texture, VkCommandBuffer cb);

// Clear the "content already synchronized" shortcut for every wlroots Vulkan
// texture currently importing the given wlr_buffer. Call from the surface
// commit path when the client (re-)attaches a buffer: the producer may have
// written new content, so the next sampling acquire must export and wait for a
// fresh producer fence instead of reusing the previous one. The buffer may be
// a wlr_client_buffer, in which case every texture importing either it or its
// source buffer is cleared. No-op for a NULL/invalid renderer, a NULL buffer,
// or a non-Vulkan renderer.
void waylib_vk_renderer_mark_buffer_content_dirty(struct wlr_renderer *renderer,
	struct wlr_buffer *buffer);

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

// Clear the frame's color-attachment-producer and sampled-texture alias
// lists. Call from the compositor frame boundary once the frame's GPU work
// has completed (e.g. after a synchronous Qt Offscreen endFrame on the same
// queue): every submission of the finished frame is ordered implicit with the
// next one on the same queue, and the in-frame alias barriers are recorded
// from the lists during the next frame, so keeping stale entries only adds
// permanent global barriers and unbounded CPU scans.
void waylib_vk_renderer_reset_frame_alias_lists(struct wlr_renderer *renderer);

// Readback ownership gate for DMA-BUF-imported wlroots textures read through
// wlr_texture_read_pixels()/vulkan_read_pixels() from outside the Qt frame.
// The import lacks a known Vulkan-side layout and may carry an unsignaled
// producer fence, so reading it without this gate uses FOREIGN-owned memory.
// vulkan_texture_read_pixels() applies this gate internally, which covers the
// wlroots protocol paths (screencopy, ext-image-copy-capture); callers that
// want the gate held around a larger cycle (WBufferDumper) may call
// begin_readback() themselves before the read: for a dmabuf texture it waits
// the buffer's DMA-BUF fences (sync_file poll) and records/submits a
// FOREIGN->own acquire barrier leaving the image in VK_IMAGE_LAYOUT_GENERAL;
// the follow-up read_pixels() staging command buffer is submitted later on the
// same queue, which gives the required execution dependency. ACQUIRED means
// end_readback() MUST be called after the read (recorded on its own command
// buffer); NOOP is returned for textures that are not DMA-BUF imports (SHM
// upload textures) and for nested reads inside an already-held cycle (the
// outer cycle records the matching release); ERROR fails closed for
// multi-plane/disjoint/YCbCr textures, a texture whose foreign ownership is
// already held by a frame, or one whose release previously failed.
// begin_readback() never waits the read itself; the read_pixels() staging
// submit CPU-waits as usual.
enum wlr_vk_texture_readback_state {
	WLR_VK_TEXTURE_READBACK_ERROR = 0,
	WLR_VK_TEXTURE_READBACK_ACQUIRED,
	WLR_VK_TEXTURE_READBACK_NOOP,
};
enum wlr_vk_texture_readback_state waylib_vk_texture_begin_readback(
	struct wlr_renderer *renderer, struct wlr_texture *texture);
void waylib_vk_texture_end_readback(struct wlr_renderer *renderer,
	struct wlr_texture *texture);

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
