// SPDX-FileCopyrightText: 2017-2023 wlroots contributors
// SPDX-License-Identifier: MIT
//
// Vendored from wlroots 0.19.3 (commit 88a869855742281c98c22cab9641b317b8d065ef)
// Source path: include/render/allocator/udmabuf.h
// Modifications: Vendored for waylib sgrenderer. Initial vendor, no functional changes.

#ifndef RENDER_ALLOCATOR_UDMABUF_H
#define RENDER_ALLOCATOR_UDMABUF_H

#include <wlr/types/wlr_buffer.h>
#include <wlr/render/allocator.h>

struct wlr_udmabuf_buffer {
	struct wlr_buffer base;

	size_t size;
	struct wlr_shm_attributes shm;
	struct wlr_dmabuf_attributes dmabuf;
};

struct wlr_udmabuf_allocator {
	struct wlr_allocator base;

	int fd;
};

struct wlr_allocator *wlr_udmabuf_allocator_create(void);

#endif
