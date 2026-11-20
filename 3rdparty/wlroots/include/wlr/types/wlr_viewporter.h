/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_VIEWPORTER_H
#define WLR_TYPES_WLR_VIEWPORTER_H

#include <stdbool.h>
#include <wayland-server-core.h>

/**
 * Implementation for the viewporter protocol.
 *
 * When enabling viewporter, compositors need to update their rendering logic:
 *
 * - The size of the surface texture may not match the surface size anymore.
 *   Compositors must use the surface size only.
 * - When rendering a surface texture, compositors should use
 *   wlr_surface_get_buffer_source_box() to get the source rectangle and crop
 *   the texture accordingly.
 */
struct wlr_viewporter {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_viewporter *wlr_viewporter_create(struct wl_display *display);

/**
 * Destroy the protocol manager and remove its globals.
 *
 * If destroy_global_safe is true, remove globals now and defer their
 * destruction; otherwise destroy them immediately.
 *
 * All client objects using this manager must be destroyed before calling this
 * function. The manager is also destroyed automatically when the display is
 * destroyed.
 */
void wlr_viewporter_destroy(struct wlr_viewporter *viewporter, bool destroy_global_safe);

#endif
