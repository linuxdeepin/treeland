#include <assert.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/vulkan.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include "render/pixel_format.h"
#include "render/vulkan.h"
#include "util/time.h"

static const struct wlr_texture_impl texture_impl;

bool wlr_texture_is_vk(struct wlr_texture *wlr_texture) {
	return wlr_texture->impl == &texture_impl;
}

struct wlr_vk_texture *vulkan_get_texture(struct wlr_texture *wlr_texture) {
	assert(wlr_texture_is_vk(wlr_texture));
	struct wlr_vk_texture *texture = wl_container_of(wlr_texture, texture, wlr_texture);
	return texture;
}

static VkImageAspectFlagBits mem_plane_aspect(unsigned i) {
	switch (i) {
	case 0: return VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
	case 1: return VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;
	case 2: return VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT;
	case 3: return VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT;
	default: abort(); // unreachable
	}
}

// Will transition the texture to shaderReadOnlyOptimal layout for reading
// from fragment shader later on
static bool write_pixels(struct wlr_vk_texture *texture,
		uint32_t stride, const pixman_region32_t *region, const void *vdata,
		VkImageLayout old_layout, VkPipelineStageFlags src_stage,
		VkAccessFlags src_access) {
	struct wlr_vk_renderer *renderer = texture->renderer;

	const struct wlr_pixel_format_info *format_info = drm_get_pixel_format_info(texture->format->drm);
	assert(format_info);

	uint32_t bsize = 0;

	// deferred upload by transfer; using staging buffer
	// calculate maximum side needed
	int rects_len = 0;
	const pixman_box32_t *rects = pixman_region32_rectangles(region, &rects_len);
	for (int i = 0; i < rects_len; i++) {
		pixman_box32_t rect = rects[i];
		uint32_t width = rect.x2 - rect.x1;
		uint32_t height = rect.y2 - rect.y1;

		// make sure assumptions are met
		assert((uint32_t)rect.x2 <= texture->wlr_texture.width);
		assert((uint32_t)rect.y2 <= texture->wlr_texture.height);

		bsize += height * pixel_format_info_min_stride(format_info, width);
	}

	VkBufferImageCopy *copies = calloc((size_t)rects_len, sizeof(*copies));
	if (!copies) {
		wlr_log(WLR_ERROR, "Failed to allocate image copy parameters");
		return false;
	}

	// get staging buffer
	struct wlr_vk_buffer_span span = vulkan_get_stage_span(renderer, bsize, format_info->bytes_per_block);
	if (!span.buffer || span.alloc.size != bsize) {
		wlr_log(WLR_ERROR, "Failed to retrieve staging buffer");
		free(copies);
		return false;
	}
	char *map = (char*)span.buffer->cpu_mapping + span.alloc.start;

	// upload data

	uint32_t buf_off = span.alloc.start;
	for (int i = 0; i < rects_len; i++) {
		pixman_box32_t rect = rects[i];
		uint32_t width = rect.x2 - rect.x1;
		uint32_t height = rect.y2 - rect.y1;
		uint32_t src_x = rect.x1;
		uint32_t src_y = rect.y1;
		uint32_t packed_stride = (uint32_t)pixel_format_info_min_stride(format_info, width);

		// write data into staging buffer span
		const char *pdata = vdata; // data iterator
		pdata += stride * src_y;
		pdata += format_info->bytes_per_block * src_x;
		if (src_x == 0 && width == texture->wlr_texture.width &&
				stride == packed_stride) {
			memcpy(map, pdata, packed_stride * height);
			map += packed_stride * height;
		} else {
			for (unsigned i = 0u; i < height; ++i) {
				memcpy(map, pdata, packed_stride);
				pdata += stride;
				map += packed_stride;
			}
		}

		copies[i] = (VkBufferImageCopy) {
			.imageExtent.width = width,
			.imageExtent.height = height,
			.imageExtent.depth = 1,
			.imageOffset.x = src_x,
			.imageOffset.y = src_y,
			.imageOffset.z = 0,
			.bufferOffset = buf_off,
			.bufferRowLength = width,
			.bufferImageHeight = height,
			.imageSubresource.mipLevel = 0,
			.imageSubresource.baseArrayLayer = 0,
			.imageSubresource.layerCount = 1,
			.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		};


		buf_off += height * packed_stride;
	}

	// record staging cb
	// will be executed before next frame
	VkCommandBuffer cb = vulkan_record_stage_cb(renderer);
	if (cb == VK_NULL_HANDLE) {
		free(copies);
		return false;
	}

	vulkan_change_layout(cb, texture->image,
		old_layout, src_stage, src_access,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT);

	vkCmdCopyBufferToImage(cb, span.buffer->buffer, texture->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)rects_len, copies);
	vulkan_change_layout(cb, texture->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_SHADER_READ_BIT);
	texture->last_used_cb = renderer->stage.cb;

	free(copies);

	return true;
}

static bool vulkan_texture_update_from_buffer(struct wlr_texture *wlr_texture,
		struct wlr_buffer *buffer, const pixman_region32_t *damage) {
	struct wlr_vk_texture *texture = vulkan_get_texture(wlr_texture);

	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		return false;
	}

	bool ok = true;

	if (format != texture->format->drm) {
		ok = false;
		goto out;
	}

	ok = write_pixels(texture, stride, damage, data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

out:
	wlr_buffer_end_data_ptr_access(buffer);
	return ok;
}

void vulkan_texture_destroy(struct wlr_vk_texture *texture) {
	if (texture->buffer != NULL) {
		wlr_addon_finish(&texture->buffer_addon);
		texture->buffer = NULL;
	}

	// when we recorded a command to fill this image _this_ frame,
	// it has to be executed before the texture can be destroyed.
	// Add it to the renderer->destroy_textures list, destroying
	// _after_ the stage command buffer has exectued
	if (texture->last_used_cb != NULL) {
		assert(texture->destroy_link.next == NULL); // not already inserted
		wl_list_insert(&texture->last_used_cb->destroy_textures,
			&texture->destroy_link);
		return;
	}

	wl_list_remove(&texture->link);

	VkDevice dev = texture->renderer->dev->dev;

	struct wlr_vk_texture_view *view, *tmp_view;
	wl_list_for_each_safe(view, tmp_view, &texture->views, link) {
		vulkan_free_ds(texture->renderer, view->ds_pool, view->ds);
		vkDestroyImageView(dev, view->image_view, NULL);
		free(view);
	}

	vkDestroyImage(dev, texture->image, NULL);

	for (unsigned i = 0u; i < texture->mem_count; ++i) {
		vkFreeMemory(dev, texture->memories[i], NULL);
	}

	free(texture);
}

static void vulkan_texture_unref(struct wlr_texture *wlr_texture) {
	struct wlr_vk_texture *texture = vulkan_get_texture(wlr_texture);
	if (texture->buffer != NULL) {
		// Keep the texture around, in case the buffer is re-used later. We're
		// still listening to the buffer's destroy event.
		wlr_buffer_unlock(texture->buffer);
	} else {
		vulkan_texture_destroy(texture);
	}
}

static bool vulkan_texture_read_pixels(struct wlr_texture *wlr_texture,
		const struct wlr_texture_read_pixels_options *options) {
	struct wlr_vk_texture *texture = vulkan_get_texture(wlr_texture);

	struct wlr_box src;
	wlr_texture_read_pixels_options_get_src_box(options, wlr_texture, &src);

	void *p = wlr_texture_read_pixel_options_get_data(options);

	return vulkan_read_pixels(texture->renderer, texture->format->vk, texture->image,
		options->format, options->stride, src.width, src.height, src.x, src.y, 0, 0, p);
}

static uint32_t vulkan_texture_preferred_read_format(struct wlr_texture *wlr_texture) {
	struct wlr_vk_texture *texture = vulkan_get_texture(wlr_texture);
	return texture->format->drm;
}

static const struct wlr_texture_impl texture_impl = {
	.update_from_buffer = vulkan_texture_update_from_buffer,
	.read_pixels = vulkan_texture_read_pixels,
	.preferred_read_format = vulkan_texture_preferred_read_format,
	.destroy = vulkan_texture_unref,
};

static struct wlr_vk_texture *vulkan_texture_create(
		struct wlr_vk_renderer *renderer, uint32_t width, uint32_t height) {
	struct wlr_vk_texture *texture = calloc(1, sizeof(*texture));
	if (texture == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return NULL;
	}
	wlr_texture_init(&texture->wlr_texture, &renderer->wlr_renderer,
		&texture_impl, width, height);
	texture->renderer = renderer;
	wl_list_insert(&renderer->textures, &texture->link);
	wl_list_init(&texture->views);
	return texture;
}

struct wlr_vk_texture_view *vulkan_texture_get_or_create_view(struct wlr_vk_texture *texture,
		const struct wlr_vk_pipeline_layout *pipeline_layout) {
	struct wlr_vk_texture_view *view;
	wl_list_for_each(view, &texture->views, link) {
		if (view->layout == pipeline_layout) {
			return view;
		}
	}

	view = calloc(1, sizeof(*view));
	if (!view) {
		return NULL;
	}

	view->layout = pipeline_layout;

	VkResult res;
	VkDevice dev = texture->renderer->dev->dev;

	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = texture->using_mutable_srgb ? texture->format->vk_srgb
			: texture->format->vk,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = texture->has_alpha || texture->format->is_ycbcr
			? VK_COMPONENT_SWIZZLE_IDENTITY
			: VK_COMPONENT_SWIZZLE_ONE,
		.subresourceRange = (VkImageSubresourceRange){
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.image = texture->image,
	};

	VkSamplerYcbcrConversionInfo ycbcr_conversion_info;
	if (texture->format->is_ycbcr) {
		assert(pipeline_layout->ycbcr.conversion != VK_NULL_HANDLE);
		ycbcr_conversion_info = (VkSamplerYcbcrConversionInfo){
			.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
			.conversion = pipeline_layout->ycbcr.conversion,
		};
		view_info.pNext = &ycbcr_conversion_info;
	}

	res = vkCreateImageView(dev, &view_info, NULL, &view->image_view);
	if (res != VK_SUCCESS) {
		free(view);
		wlr_vk_error("vkCreateImageView failed", res);
		return NULL;
	}

	view->ds_pool = vulkan_alloc_texture_ds(texture->renderer, pipeline_layout->ds, &view->ds);
	if (!view->ds_pool) {
		free(view);
		wlr_log(WLR_ERROR, "failed to allocate descriptor");
		return NULL;
	}

	VkDescriptorImageInfo ds_img_info = {
		.imageView = view->image_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet ds_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.dstSet = view->ds,
		.pImageInfo = &ds_img_info,
	};

	vkUpdateDescriptorSets(dev, 1, &ds_write, 0, NULL);

	wl_list_insert(&texture->views, &view->link);
	return view;
}

static void texture_set_format(struct wlr_vk_texture *texture,
		const struct wlr_vk_format *format, bool has_mutable_srgb) {
	texture->format = format;
	texture->using_mutable_srgb = has_mutable_srgb;
	texture->transform = !format->is_ycbcr && has_mutable_srgb ?
		WLR_VK_TEXTURE_TRANSFORM_IDENTITY : WLR_VK_TEXTURE_TRANSFORM_SRGB;

	const struct wlr_pixel_format_info *format_info =
		drm_get_pixel_format_info(format->drm);
	if (format_info != NULL) {
		texture->has_alpha = pixel_format_has_alpha(format->drm);
	} else {
		// We don't have format info for multi-planar formats
		assert(texture->format->is_ycbcr);
	}
}

static struct wlr_texture *vulkan_texture_from_pixels(
		struct wlr_vk_renderer *renderer, uint32_t drm_fmt, uint32_t stride,
		uint32_t width, uint32_t height, const void *data) {
	VkResult res;
	VkDevice dev = renderer->dev->dev;

	const struct wlr_vk_format_props *fmt =
		vulkan_format_props_from_drm(renderer->dev, drm_fmt);
	if (fmt == NULL || fmt->format.is_ycbcr) {
		char *format_name = drmGetFormatName(drm_fmt);
		wlr_log(WLR_ERROR, "Unsupported pixel format %s (0x%08"PRIX32")",
			format_name, drm_fmt);
		free(format_name);
		return NULL;
	}

	if (width > fmt->shm.max_extent.width || height > fmt->shm.max_extent.height) {
		wlr_log(WLR_ERROR, "Texture is too large to upload (%"PRIu32"x%"PRIu32" > %"PRIu32"x%"PRIu32")",
			width, height, fmt->shm.max_extent.width, fmt->shm.max_extent.height);
		return NULL;
	}

	struct wlr_vk_texture *texture = vulkan_texture_create(renderer, width, height);
	if (texture == NULL) {
		return NULL;
	}

	texture_set_format(texture, &fmt->format, fmt->shm.has_mutable_srgb);

	VkFormat view_formats[2] = {
		fmt->format.vk,
		fmt->format.vk_srgb,
	};
	VkImageFormatListCreateInfoKHR list_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
		.pViewFormats = view_formats,
		.viewFormatCount = 2,
	};
	VkImageCreateInfo img_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = texture->format->vk,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.extent = (VkExtent3D) { width, height, 1 },
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = vulkan_shm_tex_usage,
		.pNext = fmt->shm.has_mutable_srgb ? &list_info : NULL,
	};
	if (fmt->shm.has_mutable_srgb) {
		img_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	res = vkCreateImage(dev, &img_info, NULL, &texture->image);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkCreateImage failed", res);
		goto error;
	}

	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(dev, texture->image, &mem_reqs);

	int mem_type_index = vulkan_find_mem_type(renderer->dev,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem_reqs.memoryTypeBits);
	if (mem_type_index == -1) {
		wlr_log(WLR_ERROR, "failed to find suitable vulkan memory type");
		goto error;
	}

	VkMemoryAllocateInfo mem_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = mem_type_index,
	};

	res = vkAllocateMemory(dev, &mem_info, NULL, &texture->memories[0]);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkAllocatorMemory failed", res);
		goto error;
	}

	texture->mem_count = 1;
	res = vkBindImageMemory(dev, texture->image, texture->memories[0], 0);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkBindMemory failed", res);
		goto error;
	}

	pixman_region32_t region;
	pixman_region32_init_rect(&region, 0, 0, width, height);
	if (!write_pixels(texture, stride, &region, data, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0)) {
		goto error;
	}

	return &texture->wlr_texture;

error:
	vulkan_texture_destroy(texture);
	return NULL;
}

static bool is_dmabuf_disjoint(const struct wlr_dmabuf_attributes *attribs) {
	if (attribs->n_planes == 1) {
		return false;
	}

	struct stat first_stat;
	if (fstat(attribs->fd[0], &first_stat) != 0) {
		wlr_log_errno(WLR_ERROR, "fstat failed");
		return true;
	}

	for (int i = 1; i < attribs->n_planes; i++) {
		struct stat plane_stat;
		if (fstat(attribs->fd[i], &plane_stat) != 0) {
			wlr_log_errno(WLR_ERROR, "fstat failed");
			return true;
		}

		if (first_stat.st_ino != plane_stat.st_ino) {
			return true;
		}
	}

	return false;
}

VkImage vulkan_import_dmabuf(struct wlr_vk_renderer *renderer,
		const struct wlr_dmabuf_attributes *attribs,
		VkDeviceMemory mems[static WLR_DMABUF_MAX_PLANES], uint32_t *n_mems,
		bool for_render, bool *using_mutable_srgb) {
	VkResult res;
	VkDevice dev = renderer->dev->dev;
	*n_mems = 0u;

	struct wlr_vk_format_props *fmt = vulkan_format_props_from_drm(renderer->dev,
		attribs->format);
	if (fmt == NULL) {
		char *format_name = drmGetFormatName(attribs->format);
		wlr_log(WLR_ERROR, "Unsupported pixel format %s (0x%08"PRIX32")",
			format_name, attribs->format);
		free(format_name);
		return VK_NULL_HANDLE;
	}

	uint32_t plane_count = attribs->n_planes;
	assert(plane_count < WLR_DMABUF_MAX_PLANES);
	const struct wlr_vk_format_modifier_props *mod =
		vulkan_format_props_find_modifier(fmt, attribs->modifier, for_render);
	if (!mod) {
		char *format_name = drmGetFormatName(attribs->format);
		char *modifier_name = drmGetFormatModifierName(attribs->modifier);
		wlr_log(WLR_ERROR, "Format %s (0x%08"PRIX32") can't be used with modifier "
			"%s (0x%016"PRIX64")", format_name, attribs->format,
			modifier_name, attribs->modifier);
		free(format_name);
		free(modifier_name);
		return VK_NULL_HANDLE;
	}

	if ((uint32_t) attribs->width > mod->max_extent.width ||
			(uint32_t) attribs->height > mod->max_extent.height) {
		wlr_log(WLR_ERROR, "DMA-BUF is too large to import (%"PRIi32"x%"PRIi32" > %"PRIu32"x%"PRIu32")",
			attribs->width, attribs->height, mod->max_extent.width, mod->max_extent.height);
		return VK_NULL_HANDLE;
	}

	if (mod->props.drmFormatModifierPlaneCount != plane_count) {
		wlr_log(WLR_ERROR, "Number of planes (%d) does not match format (%d)",
			plane_count, mod->props.drmFormatModifierPlaneCount);
		return VK_NULL_HANDLE;
	}

	// check if we have to create the image disjoint
	bool disjoint = is_dmabuf_disjoint(attribs);
	if (disjoint && !(mod->props.drmFormatModifierTilingFeatures
			& VK_FORMAT_FEATURE_DISJOINT_BIT)) {
		wlr_log(WLR_ERROR, "Format/Modifier does not support disjoint images");
		return VK_NULL_HANDLE;
	}

	VkExternalMemoryHandleTypeFlagBits htype =
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

	VkImageCreateInfo img_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = fmt->format.vk,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.extent = (VkExtent3D) { attribs->width, attribs->height, 1 },
		.usage = for_render ? vulkan_render_usage : vulkan_dma_tex_usage,
	};
	if (disjoint) {
		img_info.flags = VK_IMAGE_CREATE_DISJOINT_BIT;
	}
	if (mod->has_mutable_srgb) {
		img_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	VkExternalMemoryImageCreateInfo eimg = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
	};
	img_info.pNext = &eimg;

	VkSubresourceLayout plane_layouts[WLR_DMABUF_MAX_PLANES] = {0};

	img_info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
	for (unsigned i = 0u; i < plane_count; ++i) {
		plane_layouts[i].offset = attribs->offset[i];
		plane_layouts[i].rowPitch = attribs->stride[i];
		plane_layouts[i].size = 0;
	}

	VkImageDrmFormatModifierExplicitCreateInfoEXT mod_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
		.drmFormatModifierPlaneCount = plane_count,
		.drmFormatModifier = mod->props.drmFormatModifier,
		.pPlaneLayouts = plane_layouts,
	};
	eimg.pNext = &mod_info;

	VkFormat view_formats[2] = {
		fmt->format.vk,
		fmt->format.vk_srgb,
	};
	VkImageFormatListCreateInfoKHR list_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
		.pViewFormats = view_formats,
		.viewFormatCount = 2,
	};
	if (mod->has_mutable_srgb) {
		mod_info.pNext = &list_info;
	}

	VkImage image;
	res = vkCreateImage(dev, &img_info, NULL, &image);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkCreateImage", res);
		return VK_NULL_HANDLE;
	}

	unsigned mem_count = disjoint ? plane_count : 1u;
	VkBindImageMemoryInfo bindi[WLR_DMABUF_MAX_PLANES] = {0};
	VkBindImagePlaneMemoryInfo planei[WLR_DMABUF_MAX_PLANES] = {0};

	for (unsigned i = 0u; i < mem_count; ++i) {
		VkMemoryFdPropertiesKHR fdp = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
		};
		res = renderer->dev->api.vkGetMemoryFdPropertiesKHR(dev, htype,
			attribs->fd[i], &fdp);
		if (res != VK_SUCCESS) {
			wlr_vk_error("getMemoryFdPropertiesKHR", res);
			goto error_image;
		}

		VkImageMemoryRequirementsInfo2 memri = {
			.image = image,
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
		};

		VkImagePlaneMemoryRequirementsInfo planeri;
		if (disjoint) {
			planeri = (VkImagePlaneMemoryRequirementsInfo){
				.sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
				.planeAspect = mem_plane_aspect(i),
			};
			memri.pNext = &planeri;
		}

		VkMemoryRequirements2 memr = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		};

		vkGetImageMemoryRequirements2(dev, &memri, &memr);
		int mem = vulkan_find_mem_type(renderer->dev, 0,
			memr.memoryRequirements.memoryTypeBits & fdp.memoryTypeBits);
		if (mem < 0) {
			wlr_log(WLR_ERROR, "no valid memory type index");
			goto error_image;
		}

		// Since importing transfers ownership of the FD to Vulkan, we have
		// to duplicate it since this operation does not transfer ownership
		// of the attribs to this texture. Will be closed by Vulkan on
		// vkFreeMemory.
		int dfd = fcntl(attribs->fd[i], F_DUPFD_CLOEXEC, 0);
		if (dfd < 0) {
			wlr_log_errno(WLR_ERROR, "fcntl(F_DUPFD_CLOEXEC) failed");
			goto error_image;
		}

		VkMemoryAllocateInfo memi = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memr.memoryRequirements.size,
			.memoryTypeIndex = mem,
		};

		VkImportMemoryFdInfoKHR importi = {
			.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
			.fd = dfd,
			.handleType = htype,
		};
		memi.pNext = &importi;

		VkMemoryDedicatedAllocateInfo dedi = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			.image = image,
		};
		importi.pNext = &dedi;

		res = vkAllocateMemory(dev, &memi, NULL, &mems[i]);
		if (res != VK_SUCCESS) {
			close(dfd);
			wlr_vk_error("vkAllocateMemory failed", res);
			goto error_image;
		}

		++(*n_mems);

		// fill bind info
		bindi[i].image = image;
		bindi[i].memory = mems[i];
		bindi[i].memoryOffset = 0;
		bindi[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;

		if (disjoint) {
			planei[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
			planei[i].planeAspect = planeri.planeAspect;
			bindi[i].pNext = &planei[i];
		}
	}

	res = vkBindImageMemory2(dev, mem_count, bindi);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkBindMemory failed", res);
		goto error_image;
	}

	*using_mutable_srgb = mod->has_mutable_srgb;
	return image;

error_image:
	vkDestroyImage(dev, image, NULL);
	for (size_t i = 0u; i < *n_mems; ++i) {
		vkFreeMemory(dev, mems[i], NULL);
		mems[i] = VK_NULL_HANDLE;
	}

	return VK_NULL_HANDLE;
}

static struct wlr_vk_texture *vulkan_texture_from_dmabuf(
		struct wlr_vk_renderer *renderer,
		struct wlr_dmabuf_attributes *attribs) {
	const struct wlr_vk_format_props *fmt = vulkan_format_props_from_drm(
		renderer->dev, attribs->format);
	if (fmt == NULL) {
		char *format_name = drmGetFormatName(attribs->format);
		wlr_log(WLR_ERROR, "Unsupported pixel format %s (0x%08"PRIX32")",
			format_name, attribs->format);
		free(format_name);
		return NULL;
	}

	struct wlr_vk_texture *texture = vulkan_texture_create(renderer,
		attribs->width, attribs->height);
	if (texture == NULL) {
		return NULL;
	}

	bool using_mutable_srgb = false;
	texture->image = vulkan_import_dmabuf(renderer, attribs,
		texture->memories, &texture->mem_count, false, &using_mutable_srgb);
	if (!texture->image) {
		goto error;
	}
	texture_set_format(texture, &fmt->format, using_mutable_srgb);

	texture->dmabuf_imported = true;

	return texture;

error:
	vulkan_texture_destroy(texture);
	return NULL;
}

static void texture_handle_buffer_destroy(struct wlr_addon *addon) {
	struct wlr_vk_texture *texture =
		wl_container_of(addon, texture, buffer_addon);
	// We might keep the texture around, waiting for pending command buffers to
	// complete before free'ing descriptor sets.
	vulkan_texture_destroy(texture);
}

static const struct wlr_addon_interface buffer_addon_impl = {
	.name = "wlr_vk_texture",
	.destroy = texture_handle_buffer_destroy,
};

static struct wlr_texture *vulkan_texture_from_dmabuf_buffer(
		struct wlr_vk_renderer *renderer, struct wlr_buffer *buffer,
		struct wlr_dmabuf_attributes *dmabuf) {
	struct wlr_addon *addon =
		wlr_addon_find(&buffer->addons, renderer, &buffer_addon_impl);
	if (addon != NULL) {
		struct wlr_vk_texture *texture =
			wl_container_of(addon, texture, buffer_addon);
		wlr_buffer_lock(texture->buffer);
		return &texture->wlr_texture;
	}

	struct wlr_vk_texture *texture = vulkan_texture_from_dmabuf(renderer, dmabuf);
	if (texture == NULL) {
		return NULL;
	}

	texture->buffer = wlr_buffer_lock(buffer);
	wlr_addon_init(&texture->buffer_addon, &buffer->addons, renderer,
		&buffer_addon_impl);

	return &texture->wlr_texture;
}

struct wlr_texture *vulkan_texture_from_buffer(struct wlr_renderer *wlr_renderer,
		struct wlr_buffer *buffer) {
	struct wlr_vk_renderer *renderer = vulkan_get_renderer(wlr_renderer);

	void *data;
	uint32_t format;
	size_t stride;
	struct wlr_dmabuf_attributes dmabuf;
	if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
		return vulkan_texture_from_dmabuf_buffer(renderer, buffer, &dmabuf);
	} else if (wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		struct wlr_texture *tex = vulkan_texture_from_pixels(renderer,
			format, stride, buffer->width, buffer->height, data);
		wlr_buffer_end_data_ptr_access(buffer);
		return tex;
	} else {
		return NULL;
	}
}

void wlr_vk_texture_get_image_attribs(struct wlr_texture *texture,
		struct wlr_vk_image_attribs *attribs) {
	struct wlr_vk_texture *vk_texture = vulkan_get_texture(texture);
	attribs->image = vk_texture->image;
	attribs->format = vk_texture->format->vk;
	attribs->layout = vk_texture->transitioned ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
}

bool wlr_vk_texture_has_alpha(struct wlr_texture *texture) {
	struct wlr_vk_texture *vk_texture = vulkan_get_texture(texture);
	return vk_texture->has_alpha;
}

static void release_completed_stage_buffers(struct wlr_vk_renderer *renderer) {
	int64_t now = get_current_time_msec();
	struct wlr_vk_shared_buffer *buf;
	wl_list_for_each(buf, &renderer->stage.buffers, link) {
		if (buf->allocs.size == 0) {
			continue;
		}

		buf->allocs.size = 0;
		buf->last_used_ms = now;
	}
}

static bool submit_pending_stage_uploads(struct wlr_vk_renderer *renderer) {
	if (renderer->stage.cb == NULL) {
		return true;
	}

	if (!vulkan_submit_stage_wait(renderer)) {
		wlr_log(WLR_ERROR, "Failed to submit pending Vulkan texture stage upload");
		return false;
	}

	release_completed_stage_buffers(renderer);
	return true;
}

static void close_sync_file_fds(int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
	for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
		if (sync_file_fds[i] < 0) {
			continue;
		}

		close(sync_file_fds[i]);
		sync_file_fds[i] = -1;
	}
}

struct wlr_vk_texture_sync_wait {
	int fd;
	size_t semaphore_index;
};

// Returns 1 when signaled, 0 on timeout, and -1 on an error or invalid poll
// result. A sync_file is only considered ready when POLLIN is reported.
static int poll_sync_file(int fd, int timeout_ms) {
	struct pollfd pollfd = {
		.fd = fd,
		.events = POLLIN,
	};

	int ret;
	do {
		ret = poll(&pollfd, 1, timeout_ms);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to poll texture DMA-BUF sync_file");
		return -1;
	}
	if (ret == 0) {
		return 0;
	}
	if ((pollfd.revents & (POLLERR | POLLNVAL)) != 0 ||
			(pollfd.revents & POLLIN) == 0) {
		wlr_log(WLR_ERROR, "Invalid texture DMA-BUF sync_file poll result: 0x%x",
			pollfd.revents);
		return -1;
	}

	return 1;
}

static bool wait_sync_file_fds(int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
	bool ok = true;

	for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
		int fd = sync_file_fds[i];
		if (fd < 0) {
			continue;
		}

		int ret = poll_sync_file(fd, 1000);
		if (ret == 0) {
			wlr_log(WLR_ERROR, "Timed out while waiting for texture DMA-BUF sync_file");
			ok = false;
		} else if (ret < 0) {
			ok = false;
		}

		close(fd);
		sync_file_fds[i] = -1;
	}

	return ok;
}

static bool collect_sync_file_fds(struct wlr_vk_renderer *renderer,
		int sync_file_fds[static WLR_DMABUF_MAX_PLANES]) {
	size_t count = 0;
	for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
		count += sync_file_fds[i] >= 0;
	}
	if (count == 0) {
		return true;
	}

	struct wlr_vk_texture_sync_wait *waits = wl_array_add(
		&renderer->texture_sync_pending, count * sizeof(*waits));
	if (waits == NULL) {
		close_sync_file_fds(sync_file_fds);
		return false;
	}

	size_t wait_index = 0;
	for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
		if (sync_file_fds[i] < 0) {
			continue;
		}

		waits[wait_index++] = (struct wlr_vk_texture_sync_wait) {
			.fd = sync_file_fds[i],
			.semaphore_index = SIZE_MAX,
		};
		sync_file_fds[i] = -1;
	}

	return true;
}

void wlr_vk_renderer_abort_texture_sync_batch(struct wlr_renderer *wlr_renderer) {
	if (wlr_renderer == NULL || !wlr_renderer_is_vk(wlr_renderer)) {
		return;
	}

	struct wlr_vk_renderer *renderer = vulkan_get_renderer(wlr_renderer);
	struct wlr_vk_texture_sync_wait *wait;
	wl_array_for_each(wait, &renderer->texture_sync_pending) {
		if (wait->fd >= 0) {
			close(wait->fd);
			wait->fd = -1;
		}
	}

	renderer->texture_sync_pending.size = 0;
	renderer->texture_sync_wait_infos.size = 0;
	renderer->texture_sync_batch_active = false;
}

bool wlr_vk_renderer_begin_texture_sync_batch(struct wlr_renderer *wlr_renderer) {
	if (wlr_renderer == NULL || !wlr_renderer_is_vk(wlr_renderer)) {
		return false;
	}

	struct wlr_vk_renderer *renderer = vulkan_get_renderer(wlr_renderer);
	if (renderer->texture_sync_batch_active) {
		wlr_log(WLR_ERROR, "Vulkan texture sync batch is already active");
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return false;
	}
	if (renderer->texture_sync_pending.size != 0) {
		wlr_log(WLR_ERROR, "Discarding stale Vulkan texture sync batch entries");
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
	}

	renderer->texture_sync_batch_active = true;
	return true;
}

static bool wait_texture_sync_entries(struct wlr_vk_texture_sync_wait *waits,
		size_t count) {
	bool ok = true;
	for (size_t i = 0; i < count; i++) {
		if (waits[i].fd < 0) {
			continue;
		}

		int ret = poll_sync_file(waits[i].fd, 1000);
		if (ret == 0) {
			wlr_log(WLR_ERROR, "Timed out while waiting for texture DMA-BUF sync_file");
			ok = false;
		} else if (ret < 0) {
			ok = false;
		}

		close(waits[i].fd);
		waits[i].fd = -1;
	}
	return ok;
}

static struct wlr_vk_texture_sync_sem *texture_sync_sem_at(
		struct wlr_vk_renderer *renderer, size_t index) {
	size_t count = renderer->texture_sync_semaphores.size /
		sizeof(struct wlr_vk_texture_sync_sem);
	assert(index < count);
	return &((struct wlr_vk_texture_sync_sem *)
		renderer->texture_sync_semaphores.data)[index];
}

// Returns an index instead of a pointer because wl_array_add() may move the
// backing allocation while multiple semaphores are claimed for one batch.
static bool acquire_texture_sync_sem(struct wlr_vk_renderer *renderer,
		uint64_t cur_point, size_t *index) {
	struct wlr_vk_texture_sync_sem *sems = renderer->texture_sync_semaphores.data;
	size_t count = renderer->texture_sync_semaphores.size / sizeof(*sems);
	for (size_t i = 0; i < count; i++) {
		if (sems[i].release_point <= cur_point) {
			sems[i].release_point = UINT64_MAX;
			*index = i;
			return true;
		}
	}

	VkSemaphoreCreateInfo semaphore_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	VkSemaphore semaphore;
	VkResult res = vkCreateSemaphore(renderer->dev->dev, &semaphore_info,
		NULL, &semaphore);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkCreateSemaphore", res);
		return false;
	}

	struct wlr_vk_texture_sync_sem *sem = wl_array_add(
		&renderer->texture_sync_semaphores, sizeof(*sem));
	if (sem == NULL) {
		vkDestroySemaphore(renderer->dev->dev, semaphore, NULL);
		return false;
	}

	*sem = (struct wlr_vk_texture_sync_sem) {
		.semaphore = semaphore,
		.release_point = UINT64_MAX,
	};
	*index = count;
	return true;
}

static void release_claimed_texture_sync_sems(struct wlr_vk_renderer *renderer,
		struct wlr_vk_texture_sync_wait *waits, size_t count) {
	for (size_t i = 0; i < count; i++) {
		if (waits[i].semaphore_index == SIZE_MAX) {
			continue;
		}
		texture_sync_sem_at(renderer, waits[i].semaphore_index)->release_point = 0;
		waits[i].semaphore_index = SIZE_MAX;
	}
}

static bool submit_texture_sync_waits(struct wlr_vk_renderer *renderer,
		struct wlr_vk_texture_sync_wait *waits,
		VkSemaphoreSubmitInfoKHR *wait_infos, size_t wait_count) {
	if (wait_count == 0) {
		return true;
	}
	if (wait_count > UINT32_MAX) {
		wlr_log(WLR_ERROR, "Too many Vulkan texture sync waits in one batch: %zu",
			wait_count);
		return false;
	}
	if (renderer->texture_sync_timeline_point == UINT64_MAX) {
		wlr_log(WLR_ERROR, "Vulkan texture sync timeline value exhausted");
		return false;
	}

	uint64_t new_point = renderer->texture_sync_timeline_point + 1;
	VkSemaphoreSubmitInfoKHR signal_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
		.semaphore = renderer->texture_sync_timeline_semaphore,
		.value = new_point,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
	};
	VkSubmitInfo2KHR submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
		.waitSemaphoreInfoCount = (uint32_t)wait_count,
		.pWaitSemaphoreInfos = wait_infos,
		.commandBufferInfoCount = 0,
		.pCommandBufferInfos = NULL,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_info,
	};
	VkResult res = renderer->dev->api.vkQueueSubmit2KHR(renderer->dev->queue,
		1, &submit, VK_NULL_HANDLE);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkQueueSubmit2KHR", res);
		return false;
	}

	renderer->texture_sync_timeline_point = new_point;
	for (size_t i = 0; i < wait_count; i++) {
		texture_sync_sem_at(renderer, waits[i].semaphore_index)->release_point = new_point;
	}

	wlr_log(WLR_DEBUG, "Vulkan foreign texture: submitted one GPU wait-only "
		"batch for %zu DMA-BUF sync_file(s), timeline point %" PRIu64,
		wait_count, new_point);
	return true;
}

bool wlr_vk_renderer_flush_texture_sync_batch(struct wlr_renderer *wlr_renderer) {
	if (wlr_renderer == NULL || !wlr_renderer_is_vk(wlr_renderer)) {
		return false;
	}

	struct wlr_vk_renderer *renderer = vulkan_get_renderer(wlr_renderer);
	if (!renderer->texture_sync_batch_active) {
		return true;
	}

	struct wlr_vk_texture_sync_wait *waits = renderer->texture_sync_pending.data;
	size_t wait_count = renderer->texture_sync_pending.size / sizeof(*waits);
	size_t pending_count = 0;
	for (size_t i = 0; i < wait_count; i++) {
		int ret = poll_sync_file(waits[i].fd, 0);
		if (ret < 0) {
			wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
			return false;
		}
		if (ret > 0) {
			close(waits[i].fd);
			waits[i].fd = -1;
			continue;
		}

		if (pending_count != i) {
			waits[pending_count] = waits[i];
			waits[i].fd = -1;
		}
		pending_count++;
	}
	renderer->texture_sync_pending.size = pending_count * sizeof(*waits);
	wait_count = pending_count;

	if (wait_count == 0) {
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return true;
	}
	if (wait_count > UINT32_MAX) {
		bool ok = wait_texture_sync_entries(waits, wait_count);
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return ok;
	}

	bool can_gpu_wait = renderer->texture_sync_timeline_semaphore != VK_NULL_HANDLE
		&& renderer->dev->api.vkImportSemaphoreFdKHR != NULL
		&& renderer->dev->api.vkQueueSubmit2KHR != NULL
		&& renderer->dev->api.vkGetSemaphoreCounterValueKHR != NULL
		&& !renderer->texture_sync_force_poll;
	if (!can_gpu_wait) {
		bool ok = wait_texture_sync_entries(waits, wait_count);
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return ok;
	}

	uint64_t cur_point = 0;
	VkResult res = renderer->dev->api.vkGetSemaphoreCounterValueKHR(
		renderer->dev->dev, renderer->texture_sync_timeline_semaphore, &cur_point);
	if (res != VK_SUCCESS) {
		wlr_vk_error("vkGetSemaphoreCounterValueKHR", res);
		bool ok = wait_texture_sync_entries(waits, wait_count);
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return ok;
	}

	renderer->texture_sync_wait_infos.size = 0;
	VkSemaphoreSubmitInfoKHR *wait_infos = wl_array_add(
		&renderer->texture_sync_wait_infos, wait_count * sizeof(*wait_infos));
	if (wait_infos == NULL) {
		bool ok = wait_texture_sync_entries(waits, wait_count);
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return ok;
	}

	size_t claimed_count = 0;
	for (; claimed_count < wait_count; claimed_count++) {
		if (!acquire_texture_sync_sem(renderer, cur_point,
				&waits[claimed_count].semaphore_index)) {
			release_claimed_texture_sync_sems(renderer, waits, claimed_count);
			bool ok = wait_texture_sync_entries(waits, wait_count);
			wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
			return ok;
		}
	}

	size_t imported_count = 0;
	for (; imported_count < wait_count; imported_count++) {
		struct wlr_vk_texture_sync_sem *sem = texture_sync_sem_at(renderer,
			waits[imported_count].semaphore_index);
		VkImportSemaphoreFdInfoKHR import_info = {
			.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
			.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
			.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT,
			.semaphore = sem->semaphore,
			.fd = waits[imported_count].fd,
		};
		res = renderer->dev->api.vkImportSemaphoreFdKHR(renderer->dev->dev,
			&import_info);
		if (res != VK_SUCCESS) {
			wlr_vk_error("vkImportSemaphoreFdKHR", res);
			break;
		}

		waits[imported_count].fd = -1; // consumed by successful import
		wait_infos[imported_count] = (VkSemaphoreSubmitInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
			.semaphore = sem->semaphore,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		};
	}

	if (imported_count < wait_count) {
		release_claimed_texture_sync_sems(renderer,
			&waits[imported_count], wait_count - imported_count);
		bool gpu_ok = submit_texture_sync_waits(renderer, waits, wait_infos,
			imported_count);
		bool cpu_ok = wait_texture_sync_entries(&waits[imported_count],
			wait_count - imported_count);
		wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
		return gpu_ok && cpu_ok;
	}

	bool ok = submit_texture_sync_waits(renderer, waits, wait_infos, wait_count);
	wlr_vk_renderer_abort_texture_sync_batch(wlr_renderer);
	return ok;
}

static void fill_sampling_attribs(struct wlr_vk_texture *texture,
		struct wlr_vk_image_attribs *attribs) {
	if (attribs == NULL) {
		return;
	}

	attribs->image = texture->image;
	attribs->format = texture->format->vk;
	attribs->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

bool wlr_vk_renderer_prepare_texture_for_sampling(struct wlr_renderer *wlr_renderer,
		struct wlr_texture *wlr_texture, VkCommandBuffer cb,
		struct wlr_vk_image_attribs *attribs) {
	if (wlr_renderer == NULL || wlr_texture == NULL || cb == VK_NULL_HANDLE) {
		return false;
	}
	if (!wlr_renderer_is_vk(wlr_renderer) || !wlr_texture_is_vk(wlr_texture)) {
		return false;
	}

	struct wlr_vk_renderer *renderer = vulkan_get_renderer(wlr_renderer);
	struct wlr_vk_texture *texture = vulkan_get_texture(wlr_texture);
	if (texture->renderer != renderer) {
		wlr_log(WLR_ERROR, "Vulkan texture belongs to a different renderer");
		return false;
	}

	if (!submit_pending_stage_uploads(renderer)) {
		return false;
	}

	if (texture->dmabuf_imported && !texture->owned) {
		int sync_file_fds[WLR_DMABUF_MAX_PLANES];
		for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
			sync_file_fds[i] = -1;
		}

		if (texture->buffer != NULL) {
			if (!vulkan_sync_foreign_texture_acquire(texture, sync_file_fds)) {
				close_sync_file_fds(sync_file_fds);
				wlr_log(WLR_ERROR, "Failed to wait for foreign texture DMA-BUF fence");
				return false;
			}

			static bool logged_foreign_sync_method;
			if (!logged_foreign_sync_method) {
				logged_foreign_sync_method = true;
				wlr_log(WLR_DEBUG, "Vulkan foreign texture: handling DMA-BUF "
					"sync_file via %s", renderer->texture_sync_batch_active
						? "deferred frame batch" : "immediate CPU poll");
			}

			bool waited = renderer->texture_sync_batch_active
				? collect_sync_file_fds(renderer, sync_file_fds)
				: wait_sync_file_fds(sync_file_fds);
			if (!waited) {
				return false;
			}
		}

		VkImageMemoryBarrier barrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
			.dstQueueFamilyIndex = renderer->dev->queue_family,
			.image = texture->image,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.layerCount = 1,
			.subresourceRange.levelCount = 1,
		};

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, NULL, 0, NULL, 1, &barrier);

		texture->owned = true;
	}

	texture->transitioned = true;
	fill_sampling_attribs(texture, attribs);
	return true;
}

bool wlr_vk_renderer_finish_texture_sampling(struct wlr_renderer *wlr_renderer,
		struct wlr_texture *wlr_texture, VkCommandBuffer cb) {
	if (wlr_renderer == NULL || wlr_texture == NULL || cb == VK_NULL_HANDLE) {
		return false;
	}
	if (!wlr_renderer_is_vk(wlr_renderer) || !wlr_texture_is_vk(wlr_texture)) {
		return false;
	}

	struct wlr_vk_renderer *renderer = vulkan_get_renderer(wlr_renderer);
	struct wlr_vk_texture *texture = vulkan_get_texture(wlr_texture);
	if (texture->renderer != renderer) {
		wlr_log(WLR_ERROR, "Vulkan texture belongs to a different renderer");
		return false;
	}

	if (!texture->dmabuf_imported || !texture->owned) {
		return true;
	}

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = renderer->dev->queue_family,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
		.image = texture->image,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL,
		.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.dstAccessMask = 0,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = 1,
	};

	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		0, NULL, 0, NULL, 1, &barrier);
	texture->owned = false;
	return true;
}
