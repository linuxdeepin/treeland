#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <wlr/render/color.h>
#include <wlr/render/drm_syncobj.h>

#include "render/color.h"
#include "render/vulkan.h"
#include "util/matrix.h"

static const struct wlr_render_pass_impl render_pass_impl;
static const struct wlr_addon_interface vk_color_transform_impl;

static struct wlr_vk_render_pass *get_render_pass(struct wlr_render_pass *wlr_pass) {
	assert(wlr_pass->impl == &render_pass_impl);
	struct wlr_vk_render_pass *pass = wl_container_of(wlr_pass, pass, base);
	return pass;
}

static struct wlr_vk_color_transform *get_color_transform(
		struct wlr_color_transform *c, struct wlr_vk_renderer *renderer) {
	struct wlr_addon *a = wlr_addon_find(&c->addons, renderer, &vk_color_transform_impl);
	if (!a) {
		return NULL;
	}
	struct wlr_vk_color_transform *transform = wl_container_of(a, transform, addon);
	return transform;
}

static void bind_pipeline(struct wlr_vk_render_pass *pass, VkPipeline pipeline) {
	if (pipeline == pass->bound_pipeline) {
		return;
	}

	vkCmdBindPipeline(pass->command_buffer->vk, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	pass->bound_pipeline = pipeline;
}

static void get_clip_region(struct wlr_vk_render_pass *pass,
		const pixman_region32_t *in, pixman_region32_t *out) {
	if (in != NULL) {
		pixman_region32_init(out);
		pixman_region32_copy(out, in);
	} else {
		struct wlr_buffer *buffer = pass->render_buffer->wlr_buffer;
		pixman_region32_init_rect(out, 0, 0, buffer->width, buffer->height);
	}
}

static void convert_pixman_box_to_vk_rect(const pixman_box32_t *box, VkRect2D *rect) {
	*rect = (VkRect2D){
		.offset = { .x = box->x1, .y = box->y1 },
		.extent = { .width = box->x2 - box->x1, .height = box->y2 - box->y1 },
	};
}

static float color_to_linear(float non_linear) {
	// See https://www.w3.org/Graphics/Color/srgb
	return (non_linear > 0.04045) ?
		pow((non_linear + 0.055) / 1.055, 2.4) :
		non_linear / 12.92;
}

static float color_to_linear_premult(float non_linear, float alpha) {
	return (alpha == 0) ? 0 : color_to_linear(non_linear / alpha) * alpha;
}

static void mat3_to_mat4(const float mat3[9], float mat4[4][4]) {
	memset(mat4, 0, sizeof(float) * 16);
	mat4[0][0] = mat3[0];
	mat4[0][1] = mat3[1];
	mat4[0][3] = mat3[2];

	mat4[1][0] = mat3[3];
	mat4[1][1] = mat3[4];
	mat4[1][3] = mat3[5];

	mat4[2][2] = 1.f;
	mat4[3][3] = 1.f;
}

static void render_pass_destroy(struct wlr_vk_render_pass *pass) {
	struct wlr_vk_render_pass_texture *pass_texture;
	wl_array_for_each(pass_texture, &pass->textures) {
		wlr_drm_syncobj_timeline_unref(pass_texture->wait_timeline);
	}

	wlr_color_transform_unref(pass->color_transform);
	wlr_drm_syncobj_timeline_unref(pass->signal_timeline);
	rect_union_finish(&pass->updated_region);
	wl_array_release(&pass->textures);
	free(pass);
}

static VkSemaphore render_pass_wait_sync_file(struct wlr_vk_render_pass *pass,
		size_t sem_index, int sync_file_fd) {
	struct wlr_vk_renderer *renderer = pass->renderer;
	struct wlr_vk_command_buffer *render_cb = pass->command_buffer;
	VkResult res;

	VkSemaphore *wait_semaphores = render_cb->wait_semaphores.data;
	size_t wait_semaphores_len = render_cb->wait_semaphores.size / sizeof(wait_semaphores[0]);

	VkSemaphore *sem_ptr;
	if (sem_index >= wait_semaphores_len) {
		sem_ptr = wl_array_add(&render_cb->wait_semaphores, sizeof(*sem_ptr));
		if (sem_ptr == NULL) {
			return VK_NULL_HANDLE;
		}
		*sem_ptr = VK_NULL_HANDLE;
	} else {
		sem_ptr = &wait_semaphores[sem_index];
	}

	if (*sem_ptr == VK_NULL_HANDLE) {
		VkSemaphoreCreateInfo semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		res = vkCreateSemaphore(renderer->dev->dev, &semaphore_info, NULL, sem_ptr);
		if (res != VK_SUCCESS) {
			wlr_vk_error("vkCreateSemaphore", res);
			return VK_NULL_HANDLE;
		}
	}

	VkImportSemaphoreFdInfoKHR import_info = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
		.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
		.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT,
		.semaphore = *sem_ptr,
		.fd = sync_file_fd,
	};
	res = renderer->dev->api.vkImportSemaphoreFdKHR(renderer->dev->dev, &import_info);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkImportSemaphoreFdKHR", res);
		return VK_NULL_HANDLE;
	}

	return *sem_ptr;
}

static bool render_pass_submit(struct wlr_render_pass *wlr_pass) {
	struct wlr_vk_render_pass *pass = get_render_pass(wlr_pass);
	struct wlr_vk_renderer *renderer = pass->renderer;
	struct wlr_vk_command_buffer *render_cb = pass->command_buffer;
	struct wlr_vk_render_buffer *render_buffer = pass->render_buffer;
	struct wlr_vk_command_buffer *stage_cb = NULL;
	VkSemaphoreSubmitInfoKHR *render_wait = NULL;
	bool device_lost = false;

	if (pass->failed) {
		goto error;
	}

	if (vulkan_record_stage_cb(renderer) == VK_NULL_HANDLE) {
		goto error;
	}

	stage_cb = renderer->stage.cb;
	assert(stage_cb != NULL);
	renderer->stage.cb = NULL;

	if (!pass->srgb_pathway) {
		// Apply output shader to map blend image to actual output image
		vkCmdNextSubpass(render_cb->vk, VK_SUBPASS_CONTENTS_INLINE);

		int width = pass->render_buffer->wlr_buffer->width;
		int height = pass->render_buffer->wlr_buffer->height;

		float final_matrix[9] = {
			width, 0, -1,
			0, height, -1,
			0, 0, 0,
		};
		struct wlr_vk_vert_pcr_data vert_pcr_data = {
			.uv_off = { 0, 0 },
			.uv_size = { 1, 1 },
		};

		size_t dim = 1;
		if (pass->color_transform && pass->color_transform->type == COLOR_TRANSFORM_LUT_3D) {
			struct wlr_color_transform_lut3d *lut3d =
				wlr_color_transform_lut3d_from_base(pass->color_transform);
			dim = lut3d->dim_len;
		}

		struct wlr_vk_frag_output_pcr_data frag_pcr_data = {
			.lut_3d_offset = 0.5f / dim,
			.lut_3d_scale = (float)(dim - 1) / dim,
		};
		mat3_to_mat4(final_matrix, vert_pcr_data.mat4);

		if (pass->color_transform) {
			bind_pipeline(pass, render_buffer->plain.render_setup->output_pipe_lut3d);
		} else {
			bind_pipeline(pass, render_buffer->plain.render_setup->output_pipe_srgb);
		}
		vkCmdPushConstants(render_cb->vk, renderer->output_pipe_layout,
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vert_pcr_data), &vert_pcr_data);
		vkCmdPushConstants(render_cb->vk, renderer->output_pipe_layout,
			VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vert_pcr_data),
			sizeof(frag_pcr_data), &frag_pcr_data);

		VkDescriptorSet lut_ds;
		if (pass->color_transform && pass->color_transform->type == COLOR_TRANSFORM_LUT_3D) {
			struct wlr_vk_color_transform *transform =
				get_color_transform(pass->color_transform, renderer);
			assert(transform);
			lut_ds = transform->lut_3d.ds;
		} else {
			lut_ds = renderer->output_ds_lut3d_dummy;
		}
		VkDescriptorSet ds[] = {
			render_buffer->plain.blend_descriptor_set, // set 0
			lut_ds, // set 1
		};
		size_t ds_len = sizeof(ds) / sizeof(ds[0]);
		vkCmdBindDescriptorSets(render_cb->vk,
			VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->output_pipe_layout,
			0, ds_len, ds, 0, NULL);

		const pixman_region32_t *clip = rect_union_evaluate(&pass->updated_region);
		int clip_rects_len;
		const pixman_box32_t *clip_rects = pixman_region32_rectangles(
			clip, &clip_rects_len);
		for (int i = 0; i < clip_rects_len; i++) {
			VkRect2D rect;
			convert_pixman_box_to_vk_rect(&clip_rects[i], &rect);
			vkCmdSetScissor(render_cb->vk, 0, 1, &rect);
			vkCmdDraw(render_cb->vk, 4, 1, 0, 0);
		}
	}

	vkCmdEndRenderPass(render_cb->vk);

	size_t pass_textures_len = pass->textures.size / sizeof(struct wlr_vk_render_pass_texture);
	size_t render_wait_cap = pass_textures_len * WLR_DMABUF_MAX_PLANES;
	render_wait = calloc(render_wait_cap, sizeof(*render_wait));
	if (render_wait == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		goto error;
	}

	uint32_t barrier_count = wl_list_length(&renderer->foreign_textures) + 1;
	VkImageMemoryBarrier *acquire_barriers = calloc(barrier_count, sizeof(*acquire_barriers));
	VkImageMemoryBarrier *release_barriers = calloc(barrier_count, sizeof(*release_barriers));
	if (acquire_barriers == NULL || release_barriers == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		free(acquire_barriers);
		free(release_barriers);
		goto error;
	}

	struct wlr_vk_texture *texture, *tmp_tex;
	size_t idx = 0;
	wl_list_for_each_safe(texture, tmp_tex, &renderer->foreign_textures, foreign_link) {
		if (!texture->transitioned) {
			texture->transitioned = true;
		}

		// acquire
		acquire_barriers[idx] = (VkImageMemoryBarrier){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
			.dstQueueFamilyIndex = renderer->dev->queue_family,
			.image = texture->image,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcAccessMask = 0, // ignored anyways
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.layerCount = 1,
			.subresourceRange.levelCount = 1,
		};

		// release
		release_barriers[idx] = (VkImageMemoryBarrier){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex = renderer->dev->queue_family,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
			.image = texture->image,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = 0, // ignored anyways
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.layerCount = 1,
			.subresourceRange.levelCount = 1,
		};

		++idx;

		wl_list_remove(&texture->foreign_link);
		texture->owned = false;
	}

	uint32_t render_wait_len = 0;
	struct wlr_vk_render_pass_texture *pass_texture;
	wl_array_for_each(pass_texture, &pass->textures) {
		int sync_file_fds[WLR_DMABUF_MAX_PLANES];
		for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
			sync_file_fds[i] = -1;
		}

		if (pass_texture->wait_timeline) {
			int sync_file_fd = wlr_drm_syncobj_timeline_export_sync_file(pass_texture->wait_timeline, pass_texture->wait_point);
			if (sync_file_fd < 0) {
				wlr_log(WLR_ERROR, "Failed to export wait timeline point as sync_file");
				continue;
			}

			sync_file_fds[0] = sync_file_fd;
		} else {
			struct wlr_vk_texture *texture = pass_texture->texture;
			if (!vulkan_sync_foreign_texture(texture, sync_file_fds)) {
				wlr_log(WLR_ERROR, "Failed to wait for foreign texture DMA-BUF fence");
				continue;
			}
		}

		for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
			if (sync_file_fds[i] < 0) {
				continue;
			}

			VkSemaphore sem = render_pass_wait_sync_file(pass, render_wait_len, sync_file_fds[i]);
			if (sem == VK_NULL_HANDLE) {
				close(sync_file_fds[i]);
				continue;
			}

			render_wait[render_wait_len] = (VkSemaphoreSubmitInfoKHR){
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
				.semaphore = sem,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
			};

			render_wait_len++;
		}
	}

	// also add acquire/release barriers for the current render buffer
	VkImageLayout src_layout = VK_IMAGE_LAYOUT_GENERAL;
	if (pass->srgb_pathway) {
		if (!render_buffer->srgb.transitioned) {
			src_layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			render_buffer->srgb.transitioned = true;
		}
	} else {
		if (!render_buffer->plain.transitioned) {
			src_layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			render_buffer->plain.transitioned = true;
		}
		// The render pass changes the blend image layout from
		// color attachment to read only, so on each frame, before
		// the render pass starts, we change it back
		VkImageLayout blend_src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		if (!render_buffer->plain.blend_transitioned) {
			blend_src_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			render_buffer->plain.blend_transitioned = true;
		}

		VkImageMemoryBarrier blend_acq_barrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = render_buffer->plain.blend_image,
			.oldLayout = blend_src_layout,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
				.levelCount = 1,
			},
		};
		vkCmdPipelineBarrier(stage_cb->vk, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, NULL, 0, NULL, 1, &blend_acq_barrier);
	}

	// acquire render buffer before rendering
	acquire_barriers[idx] = (VkImageMemoryBarrier){
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
		.dstQueueFamilyIndex = renderer->dev->queue_family,
		.image = render_buffer->image,
		.oldLayout = src_layout,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL,
		.srcAccessMask = 0, // ignored anyways
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = 1,
	};

	// release render buffer after rendering
	release_barriers[idx] = (VkImageMemoryBarrier){
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = renderer->dev->queue_family,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
		.image = render_buffer->image,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = 0, // ignored anyways
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = 1,
	};

	++idx;

	vkCmdPipelineBarrier(stage_cb->vk, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, NULL, 0, NULL, barrier_count, acquire_barriers);

	vkCmdPipelineBarrier(render_cb->vk, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL,
		barrier_count, release_barriers);

	free(acquire_barriers);
	free(release_barriers);

	// No semaphores needed here.
	// We don't need a semaphore from the stage/transfer submission
	// to the render submissions since they are on the same queue
	// and we have a renderpass dependency for that.
	uint64_t stage_timeline_point = vulkan_end_command_buffer(stage_cb, renderer);
	if (stage_timeline_point == 0) {
		goto error;
	}

	VkCommandBufferSubmitInfoKHR stage_cb_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
		.commandBuffer = stage_cb->vk,
	};
	VkSemaphoreSubmitInfoKHR stage_signal = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
		.semaphore = renderer->timeline_semaphore,
		.value = stage_timeline_point,
	};
	VkSubmitInfo2KHR stage_submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &stage_cb_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &stage_signal,
	};

	VkSemaphoreSubmitInfoKHR stage_wait;
	if (renderer->stage.last_timeline_point > 0) {
		stage_wait = (VkSemaphoreSubmitInfoKHR){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
			.semaphore = renderer->timeline_semaphore,
			.value = renderer->stage.last_timeline_point,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
		};

		stage_submit.waitSemaphoreInfoCount = 1;
		stage_submit.pWaitSemaphoreInfos = &stage_wait;
	}

	renderer->stage.last_timeline_point = stage_timeline_point;

	uint64_t render_timeline_point = vulkan_end_command_buffer(render_cb, renderer);
	if (render_timeline_point == 0) {
		goto error;
	}

	uint32_t render_signal_len = 1;
	VkSemaphoreSubmitInfoKHR render_signal[2] = {0};
	render_signal[0] = (VkSemaphoreSubmitInfoKHR){
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
		.semaphore = renderer->timeline_semaphore,
		.value = render_timeline_point,
	};
	if (renderer->dev->implicit_sync_interop || pass->signal_timeline != NULL) {
		if (render_cb->binary_semaphore == VK_NULL_HANDLE) {
			VkExportSemaphoreCreateInfo export_info = {
				.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
				.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
			};
			VkSemaphoreCreateInfo semaphore_info = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = &export_info,
			};
			VkResult res = vkCreateSemaphore(renderer->dev->dev, &semaphore_info,
				NULL, &render_cb->binary_semaphore);
			if (res != VK_SUCCESS) {
				wlr_vk_error("vkCreateSemaphore", res);
				goto error;
			}
		}

		render_signal[render_signal_len++] = (VkSemaphoreSubmitInfoKHR){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
			.semaphore = render_cb->binary_semaphore,
		};
	}

	VkCommandBufferSubmitInfoKHR render_cb_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
		.commandBuffer = render_cb->vk,
	};
	VkSubmitInfo2KHR render_submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
		.waitSemaphoreInfoCount = render_wait_len,
		.pWaitSemaphoreInfos = render_wait,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &render_cb_info,
		.signalSemaphoreInfoCount = render_signal_len,
		.pSignalSemaphoreInfos = render_signal,
	};

	VkSubmitInfo2KHR submit_info[] = { stage_submit, render_submit };
	VkResult res = renderer->dev->api.vkQueueSubmit2KHR(renderer->dev->queue, 2, submit_info, VK_NULL_HANDLE);

	if (res != VK_SUCCESS) {
		device_lost = res == VK_ERROR_DEVICE_LOST;
		wlr_vk_error("vkQueueSubmit", res);
		goto error;
	}

	free(render_wait);

	struct wlr_vk_shared_buffer *stage_buf, *stage_buf_tmp;
	wl_list_for_each_safe(stage_buf, stage_buf_tmp, &renderer->stage.buffers, link) {
		if (stage_buf->allocs.size == 0) {
			continue;
		}
		wl_list_remove(&stage_buf->link);
		wl_list_insert(&stage_cb->stage_buffers, &stage_buf->link);
	}

	if (!vulkan_sync_render_buffer(renderer, render_buffer, render_cb,
			pass->signal_timeline, pass->signal_point)) {
		wlr_log(WLR_ERROR, "Failed to sync render buffer");
	}

	render_pass_destroy(pass);
	wlr_buffer_unlock(render_buffer->wlr_buffer);
	return true;

error:
	free(render_wait);
	vulkan_reset_command_buffer(stage_cb);
	vulkan_reset_command_buffer(render_cb);
	wlr_buffer_unlock(render_buffer->wlr_buffer);
	render_pass_destroy(pass);

	if (device_lost) {
		wl_signal_emit_mutable(&renderer->wlr_renderer.events.lost, NULL);
	}

	return false;
}

static void render_pass_mark_box_updated(struct wlr_vk_render_pass *pass,
		const struct wlr_box *box) {
	if (pass->srgb_pathway) {
		return;
	}

	pixman_box32_t pixman_box = {
		.x1 = box->x,
		.x2 = box->x + box->width,
		.y1 = box->y,
		.y2 = box->y + box->height,
	};
	rect_union_add(&pass->updated_region, pixman_box);
}

static void render_pass_add_rect(struct wlr_render_pass *wlr_pass,
		const struct wlr_render_rect_options *options) {
	struct wlr_vk_render_pass *pass = get_render_pass(wlr_pass);
	VkCommandBuffer cb = pass->command_buffer->vk;

	// Input color values are given in sRGB space, shader expects
	// them in linear space. The shader does all computation in linear
	// space and expects in inputs in linear space since it outputs
	// colors in linear space as well (and vulkan then automatically
	// does the conversion for out sRGB render targets).
	float linear_color[] = {
		color_to_linear_premult(options->color.r, options->color.a),
		color_to_linear_premult(options->color.g, options->color.a),
		color_to_linear_premult(options->color.b, options->color.a),
		options->color.a, // no conversion for alpha
	};

	pixman_region32_t clip;
	get_clip_region(pass, options->clip, &clip);

	int clip_rects_len;
	const pixman_box32_t *clip_rects = pixman_region32_rectangles(&clip, &clip_rects_len);
	// Record regions possibly updated for use in second subpass
	for (int i = 0; i < clip_rects_len; i++) {
		struct wlr_box clip_box = {
			.x = clip_rects[i].x1,
			.y = clip_rects[i].y1,
			.width = clip_rects[i].x2 - clip_rects[i].x1,
			.height = clip_rects[i].y2 - clip_rects[i].y1,
		};
		struct wlr_box intersection;
		if (!wlr_box_intersection(&intersection, &options->box, &clip_box)) {
			continue;
		}
		render_pass_mark_box_updated(pass, &intersection);
	}

	struct wlr_box box;
	wlr_render_rect_options_get_box(options, pass->render_buffer->wlr_buffer, &box);

	switch (options->blend_mode) {
	case WLR_RENDER_BLEND_MODE_PREMULTIPLIED:;
		float proj[9], matrix[9];
		wlr_matrix_identity(proj);
		wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, proj);
		wlr_matrix_multiply(matrix, pass->projection, matrix);

		struct wlr_vk_render_format_setup *setup = pass->srgb_pathway ?
			pass->render_buffer->srgb.render_setup :
			pass->render_buffer->plain.render_setup;
		struct wlr_vk_pipeline *pipe = setup_get_or_create_pipeline(
			setup,
			&(struct wlr_vk_pipeline_key) {
				.source = WLR_VK_SHADER_SOURCE_SINGLE_COLOR,
				.layout = { .ycbcr_format = NULL },
			});
		if (!pipe) {
			pass->failed = true;
			break;
		}

		struct wlr_vk_vert_pcr_data vert_pcr_data = {
			.uv_off = { 0, 0 },
			.uv_size = { 1, 1 },
		};
		mat3_to_mat4(matrix, vert_pcr_data.mat4);

		bind_pipeline(pass, pipe->vk);
		vkCmdPushConstants(cb, pipe->layout->vk,
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vert_pcr_data), &vert_pcr_data);
		vkCmdPushConstants(cb, pipe->layout->vk,
			VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vert_pcr_data), sizeof(float) * 4,
			linear_color);

		for (int i = 0; i < clip_rects_len; i++) {
			VkRect2D rect;
			convert_pixman_box_to_vk_rect(&clip_rects[i], &rect);
			vkCmdSetScissor(cb, 0, 1, &rect);
			vkCmdDraw(cb, 4, 1, 0, 0);
		}
		break;
	case WLR_RENDER_BLEND_MODE_NONE:;
		VkClearAttachment clear_att = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.colorAttachment = 0,
			.clearValue.color.float32 = {
				linear_color[0],
				linear_color[1],
				linear_color[2],
				linear_color[3],
			},
		};
		VkClearRect clear_rect = {
			.layerCount = 1,
		};
		for (int i = 0; i < clip_rects_len; i++) {
			convert_pixman_box_to_vk_rect(&clip_rects[i], &clear_rect.rect);
			vkCmdClearAttachments(cb, 1, &clear_att, 1, &clear_rect);
		}
		break;
	}

	pixman_region32_fini(&clip);
}

static void render_pass_add_texture(struct wlr_render_pass *wlr_pass,
		const struct wlr_render_texture_options *options) {
	struct wlr_vk_render_pass *pass = get_render_pass(wlr_pass);
	struct wlr_vk_renderer *renderer = pass->renderer;
	VkCommandBuffer cb = pass->command_buffer->vk;

	struct wlr_vk_texture *texture = vulkan_get_texture(options->texture);
	assert(texture->renderer == renderer);

	if (texture->dmabuf_imported && !texture->owned) {
		// Store this texture in the list of textures that need to be
		// acquired before rendering and released after rendering.
		// We don't do it here immediately since barriers inside
		// a renderpass are suboptimal (would require additional renderpass
		// dependency and potentially multiple barriers) and it's
		// better to issue one barrier for all used textures anyways.
		texture->owned = true;
		assert(texture->foreign_link.prev == NULL);
		assert(texture->foreign_link.next == NULL);
		wl_list_insert(&renderer->foreign_textures, &texture->foreign_link);
	}

	struct wlr_fbox src_box;
	wlr_render_texture_options_get_src_box(options, &src_box);
	struct wlr_box dst_box;
	wlr_render_texture_options_get_dst_box(options, &dst_box);
	float alpha = wlr_render_texture_options_get_alpha(options);

	float proj[9], matrix[9];
	wlr_matrix_identity(proj);
	wlr_matrix_project_box(matrix, &dst_box, options->transform, proj);
	wlr_matrix_multiply(matrix, pass->projection, matrix);

	struct wlr_vk_vert_pcr_data vert_pcr_data = {
		.uv_off = {
			src_box.x / options->texture->width,
			src_box.y / options->texture->height,
		},
		.uv_size = {
			src_box.width / options->texture->width,
			src_box.height / options->texture->height,
		},
	};
	mat3_to_mat4(matrix, vert_pcr_data.mat4);

	struct wlr_vk_render_format_setup *setup = pass->srgb_pathway ?
		pass->render_buffer->srgb.render_setup :
		pass->render_buffer->plain.render_setup;
	struct wlr_vk_pipeline *pipe = setup_get_or_create_pipeline(
		setup,
		&(struct wlr_vk_pipeline_key) {
			.source = WLR_VK_SHADER_SOURCE_TEXTURE,
			.layout = {
				.ycbcr_format = texture->format->is_ycbcr ? texture->format : NULL,
				.filter_mode = options->filter_mode,
			},
			.texture_transform = texture->transform,
			.blend_mode = !texture->has_alpha && alpha == 1.0 ?
				WLR_RENDER_BLEND_MODE_NONE : options->blend_mode,
		});
	if (!pipe) {
		pass->failed = true;
		return;
	}

	struct wlr_vk_texture_view *view =
		vulkan_texture_get_or_create_view(texture, pipe->layout);
	if (!view) {
		pass->failed = true;
		return;
	}

	bind_pipeline(pass, pipe->vk);

	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipe->layout->vk, 0, 1, &view->ds, 0, NULL);

	vkCmdPushConstants(cb, pipe->layout->vk,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vert_pcr_data), &vert_pcr_data);
	vkCmdPushConstants(cb, pipe->layout->vk,
		VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vert_pcr_data), sizeof(float),
		&alpha);

	pixman_region32_t clip;
	get_clip_region(pass, options->clip, &clip);

	int clip_rects_len;
	const pixman_box32_t *clip_rects = pixman_region32_rectangles(&clip, &clip_rects_len);
	for (int i = 0; i < clip_rects_len; i++) {
		VkRect2D rect;
		convert_pixman_box_to_vk_rect(&clip_rects[i], &rect);
		vkCmdSetScissor(cb, 0, 1, &rect);
		vkCmdDraw(cb, 4, 1, 0, 0);

		struct wlr_box clip_box = {
			.x = clip_rects[i].x1,
			.y = clip_rects[i].y1,
			.width = clip_rects[i].x2 - clip_rects[i].x1,
			.height = clip_rects[i].y2 - clip_rects[i].y1,
		};
		struct wlr_box intersection;
		if (!wlr_box_intersection(&intersection, &dst_box, &clip_box)) {
			continue;
		}
		render_pass_mark_box_updated(pass, &intersection);
	}

	texture->last_used_cb = pass->command_buffer;

	pixman_region32_fini(&clip);

	if (texture->dmabuf_imported || (options != NULL && options->wait_timeline != NULL)) {
		struct wlr_vk_render_pass_texture *pass_texture =
			wl_array_add(&pass->textures, sizeof(*pass_texture));
		if (pass_texture == NULL) {
			pass->failed = true;
			return;
		}

		struct wlr_drm_syncobj_timeline *wait_timeline = NULL;
		uint64_t wait_point = 0;
		if (options != NULL && options->wait_timeline != NULL) {
			wait_timeline = wlr_drm_syncobj_timeline_ref(options->wait_timeline);
			wait_point = options->wait_point;
		}

		*pass_texture = (struct wlr_vk_render_pass_texture){
			.texture = texture,
			.wait_timeline = wait_timeline,
			.wait_point = wait_point,
		};
	}
}

static const struct wlr_render_pass_impl render_pass_impl = {
	.submit = render_pass_submit,
	.add_rect = render_pass_add_rect,
	.add_texture = render_pass_add_texture,
};


void vk_color_transform_destroy(struct wlr_addon *addon) {
	struct wlr_vk_renderer *renderer = (struct wlr_vk_renderer *)addon->owner;
	struct wlr_vk_color_transform *transform = wl_container_of(addon, transform, addon);

	VkDevice dev = renderer->dev->dev;
	if (transform->lut_3d.image) {
		vkDestroyImage(dev, transform->lut_3d.image, NULL);
		vkDestroyImageView(dev, transform->lut_3d.image_view, NULL);
		vkFreeMemory(dev, transform->lut_3d.memory, NULL);
		vulkan_free_ds(renderer, transform->lut_3d.ds_pool, transform->lut_3d.ds);
	}

	wl_list_remove(&transform->link);
	wlr_addon_finish(&transform->addon);
	free(transform);
}

static bool create_3d_lut_image(struct wlr_vk_renderer *renderer,
		const struct wlr_color_transform_lut3d *lut_3d,
		VkImage *image, VkImageView *image_view,
		VkDeviceMemory *memory,	VkDescriptorSet *ds,
		struct wlr_vk_descriptor_pool **ds_pool) {
	VkDevice dev = renderer->dev->dev;
	VkResult res;

	*image = VK_NULL_HANDLE;
	*memory = VK_NULL_HANDLE;
	*image_view = VK_NULL_HANDLE;
	*ds = VK_NULL_HANDLE;
	*ds_pool = NULL;

	// R32G32B32 is not a required Vulkan format
	// TODO: use it when available
	VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

	VkImageCreateInfo img_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_3D,
		.format = format,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.extent = (VkExtent3D) { lut_3d->dim_len, lut_3d->dim_len, lut_3d->dim_len },
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	};
	res = vkCreateImage(dev, &img_info, NULL, image);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkCreateImage failed", res);
		return NULL;
	}

	VkMemoryRequirements mem_reqs = {0};
	vkGetImageMemoryRequirements(dev, *image, &mem_reqs);

	int mem_type_index = vulkan_find_mem_type(renderer->dev,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem_reqs.memoryTypeBits);
	if (mem_type_index == -1) {
		wlr_log(WLR_ERROR, "Failed to find suitable memory type");
		goto fail_image;
	}

	VkMemoryAllocateInfo mem_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = mem_type_index,
	};
	res = vkAllocateMemory(dev, &mem_info, NULL, memory);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkAllocateMemory failed", res);
		goto fail_image;
	}

	res = vkBindImageMemory(dev, *image, *memory, 0);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkBindMemory failed", res);
		goto fail_memory;
	}

	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_3D,
		.format = format,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.image = *image,
	};
	res = vkCreateImageView(dev, &view_info, NULL, image_view);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkCreateImageView failed", res);
		goto fail_image;
	}

	size_t bytes_per_block = 4 * sizeof(float);
	size_t size = lut_3d->dim_len * lut_3d->dim_len * lut_3d->dim_len * bytes_per_block;
	struct wlr_vk_buffer_span span = vulkan_get_stage_span(renderer,
		size, bytes_per_block);
	if (!span.buffer || span.alloc.size != size) {
		wlr_log(WLR_ERROR, "Failed to retrieve staging buffer");
		goto fail_imageview;
	}

	char *map = (char*)span.buffer->cpu_mapping + span.alloc.start;
	float *dst = (float*)map;
	size_t dim_len = lut_3d->dim_len;
	for (size_t b_index = 0; b_index < dim_len; b_index++) {
		for (size_t g_index = 0; g_index < dim_len; g_index++) {
			for (size_t r_index = 0; r_index < dim_len; r_index++) {
				size_t sample_index = r_index + dim_len * g_index + dim_len * dim_len * b_index;
				size_t src_offset = 3 * sample_index;
				size_t dst_offset = 4 * sample_index;
				dst[dst_offset] = lut_3d->lut_3d[src_offset];
				dst[dst_offset + 1] = lut_3d->lut_3d[src_offset + 1];
				dst[dst_offset + 2] = lut_3d->lut_3d[src_offset + 2];
				dst[dst_offset + 3] = 1.0;
			}
		}
	}

	VkCommandBuffer cb = vulkan_record_stage_cb(renderer);
	vulkan_change_layout(cb, *image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT);
	VkBufferImageCopy copy = {
		.bufferOffset = span.alloc.start,
		.imageExtent.width = lut_3d->dim_len,
		.imageExtent.height = lut_3d->dim_len,
		.imageExtent.depth = lut_3d->dim_len,
		.imageSubresource.layerCount = 1,
		.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	};
	vkCmdCopyBufferToImage(cb, span.buffer->buffer, *image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	vulkan_change_layout(cb, *image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_SHADER_READ_BIT);

	*ds_pool = vulkan_alloc_texture_ds(renderer,
		renderer->output_ds_lut3d_layout, ds);
	if (!*ds_pool) {
		wlr_log(WLR_ERROR, "Failed to allocate descriptor");
		goto fail_imageview;
	}

	VkDescriptorImageInfo ds_img_info = {
		.imageView = *image_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet ds_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.dstSet = *ds,
		.pImageInfo = &ds_img_info,
	};
	vkUpdateDescriptorSets(dev, 1, &ds_write, 0, NULL);

	return true;

fail_imageview:
	vkDestroyImageView(dev, *image_view, NULL);
fail_memory:
	vkFreeMemory(dev, *memory, NULL);
fail_image:
	vkDestroyImage(dev, *image, NULL);
	return false;
}

static struct wlr_vk_color_transform *vk_color_transform_create(
		struct wlr_vk_renderer *renderer, struct wlr_color_transform *transform) {
	struct wlr_vk_color_transform *vk_transform =
		calloc(1, sizeof(*vk_transform));
	if (!vk_transform) {
		return NULL;
	}

	if (transform->type == COLOR_TRANSFORM_LUT_3D) {
		if (!create_3d_lut_image(renderer,
				wlr_color_transform_lut3d_from_base(transform),
				&vk_transform->lut_3d.image,
				&vk_transform->lut_3d.image_view,
				&vk_transform->lut_3d.memory,
				&vk_transform->lut_3d.ds,
				&vk_transform->lut_3d.ds_pool)) {
			free(vk_transform);
			return NULL;
		}
	}

	wlr_addon_init(&vk_transform->addon, &transform->addons,
		renderer, &vk_color_transform_impl);
	wl_list_insert(&renderer->color_transforms, &vk_transform->link);

	return vk_transform;
}


static const struct wlr_addon_interface vk_color_transform_impl = {
	"vk_color_transform",
	.destroy = vk_color_transform_destroy,
};

struct wlr_vk_render_pass *vulkan_begin_render_pass(struct wlr_vk_renderer *renderer,
		struct wlr_vk_render_buffer *buffer, const struct wlr_buffer_pass_options *options) {
	bool using_srgb_pathway;
	if (options != NULL && options->color_transform != NULL) {
		using_srgb_pathway = false;

		if (!get_color_transform(options->color_transform, renderer)) {
			/* Try to create a new color transform */
			if (!vk_color_transform_create(renderer, options->color_transform)) {
				wlr_log(WLR_ERROR, "Failed to create color transform");
				return NULL;
			}
		}
	} else {
		// Use srgb pathway if it is the default/has already been set up
		using_srgb_pathway = buffer->srgb.framebuffer != VK_NULL_HANDLE;
	}

	if (!using_srgb_pathway && !buffer->plain.image_view) {
		struct wlr_dmabuf_attributes attribs;
		wlr_buffer_get_dmabuf(buffer->wlr_buffer, &attribs);
		if (!vulkan_setup_plain_framebuffer(buffer, &attribs)) {
			wlr_log(WLR_ERROR, "Failed to set up blend image");
			return NULL;
		}
	}

	struct wlr_vk_render_pass *pass = calloc(1, sizeof(*pass));
	if (pass == NULL) {
		return NULL;
	}

	wlr_render_pass_init(&pass->base, &render_pass_impl);
	pass->renderer = renderer;
	pass->srgb_pathway = using_srgb_pathway;
	if (options != NULL && options->color_transform != NULL) {
		pass->color_transform = wlr_color_transform_ref(options->color_transform);
	}
	if (options != NULL && options->signal_timeline != NULL) {
		pass->signal_timeline = wlr_drm_syncobj_timeline_ref(options->signal_timeline);
		pass->signal_point = options->signal_point;
	}

	rect_union_init(&pass->updated_region);

	struct wlr_vk_command_buffer *cb = vulkan_acquire_command_buffer(renderer);
	if (cb == NULL) {
		free(pass);
		return NULL;
	}

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	VkResult res = vkBeginCommandBuffer(cb->vk, &begin_info);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkBeginCommandBuffer", res);
		vulkan_reset_command_buffer(cb);
		free(pass);
		return NULL;
	}

	if (!renderer->dummy3d_image_transitioned) {
		renderer->dummy3d_image_transitioned = true;
		vulkan_change_layout(cb->vk, renderer->dummy3d_image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_SHADER_READ_BIT);
	}

	int width = buffer->wlr_buffer->width;
	int height = buffer->wlr_buffer->height;
	VkRect2D rect = { .extent = { width, height } };

	VkRenderPassBeginInfo rp_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderArea = rect,
		.clearValueCount = 0,
	};
	if (pass->srgb_pathway) {
		rp_info.renderPass = buffer->srgb.render_setup->render_pass;
		rp_info.framebuffer = buffer->srgb.framebuffer;
	} else {
		rp_info.renderPass = buffer->plain.render_setup->render_pass;
		rp_info.framebuffer = buffer->plain.framebuffer;
	}
	vkCmdBeginRenderPass(cb->vk, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(cb->vk, 0, 1, &(VkViewport){
		.width = width,
		.height = height,
		.maxDepth = 1,
	});

	// matrix_projection() assumes a GL coordinate system so we need
	// to pass WL_OUTPUT_TRANSFORM_FLIPPED_180 to adjust it for vulkan.
	matrix_projection(pass->projection, width, height, WL_OUTPUT_TRANSFORM_FLIPPED_180);

	wlr_buffer_lock(buffer->wlr_buffer);
	pass->render_buffer = buffer;
	pass->command_buffer = cb;
	return pass;
}
