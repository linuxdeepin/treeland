// Copyright (c) 2017, 2018 Drew DeVault
// Copyright (c) 2014 Jari Vetoniemi
// Copyright (c) 2023 The wlroots contributors
// SPDX-License-Identifier: MIT

/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_H
#define WLR_TYPES_WLR_EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_H

#include <wayland-server-core.h>

struct wlr_ext_foreign_toplevel_handle_v1;
struct wlr_ext_image_capture_source_v1;
struct wlr_scene_node;
struct wlr_allocator;
struct wlr_renderer;

/**
 * Interface exposing one screen capture source per foreign toplevel.
 */
struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal new_request; // struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request {
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel_handle;
	struct wl_client *client;

	struct {
		uint32_t new_id;
	} WLR_PRIVATE;
};

/**
 * Create a new ext_foreign_toplevel_image_capture_source_manager_v1 global.
 */
struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *
wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(struct wl_display *display, uint32_t version);

/**
 * Accept a request to create a new image capture source for a foreign toplevel.
 * 
 * @param request The request from the client
 * @param source The image capture source to associate with the request
 * @return true if the request was accepted successfully, false otherwise
 */
bool wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request,
	struct wlr_ext_image_capture_source_v1 *source);

/**
 * Create a new image capture source backed by a scene node.
 * 
 * @param node The scene node to capture
 * @param event_loop The event loop for scheduling frames
 * @param allocator The allocator for creating buffers
 * @param renderer The renderer for processing frames
 * @return A new image capture source, or NULL on failure
 */
struct wlr_ext_image_capture_source_v1 *wlr_ext_image_capture_source_v1_create_with_scene_node(
	struct wlr_scene_node *node, struct wl_event_loop *event_loop,
	struct wlr_allocator *allocator, struct wlr_renderer *renderer);

#endif
