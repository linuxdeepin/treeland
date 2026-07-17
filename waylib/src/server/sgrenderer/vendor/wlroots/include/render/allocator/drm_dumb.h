// SPDX-FileCopyrightText: 2017-2023 wlroots contributors
// SPDX-License-Identifier: MIT
//
// Vendored from wlroots 0.19.3 (commit 88a869855742281c98c22cab9641b317b8d065ef)
// Source path: include/render/allocator/drm_dumb.h
// Modifications: Vendored for waylib sgrenderer. Initial vendor, no functional changes.

#ifndef RENDER_ALLOCATOR_DRM_DUMB_H
#define RENDER_ALLOCATOR_DRM_DUMB_H

#include <wlr/render/allocator.h>
#include <wlr/render/dmabuf.h>
#include <wlr/types/wlr_buffer.h>

struct wlr_drm_dumb_buffer {
	struct wlr_buffer base;
	struct wl_list link; // wlr_drm_dumb_allocator.buffers

	int drm_fd; // -1 if the allocator has been destroyed
	struct wlr_dmabuf_attributes dmabuf;

	uint32_t format;
	uint32_t handle;
	uint32_t stride;
	uint32_t width, height;

	uint64_t size;
	void *data;
};

struct wlr_drm_dumb_allocator {
	struct wlr_allocator base;
	struct wl_list buffers; // wlr_drm_dumb_buffer.link
	int drm_fd;
};

/**
 * Creates a new drm dumb allocator from a DRM FD.
 *
 * Does not take ownership over the FD.
 */
struct wlr_allocator *wlr_drm_dumb_allocator_create(int fd);

#endif
