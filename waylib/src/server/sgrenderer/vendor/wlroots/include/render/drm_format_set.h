// SPDX-FileCopyrightText: 2017-2023 wlroots contributors
// SPDX-License-Identifier: MIT
//
// Vendored from wlroots 0.19.3 (commit 88a869855742281c98c22cab9641b317b8d065ef)
// Source path: include/render/drm_format_set.h
// Modifications: Vendored for waylib sgrenderer. Initial vendor, no functional changes.

#ifndef RENDER_DRM_FORMAT_SET_H
#define RENDER_DRM_FORMAT_SET_H

#include <wlr/render/drm_format_set.h>

void wlr_drm_format_init(struct wlr_drm_format *fmt, uint32_t format);
bool wlr_drm_format_has(const struct wlr_drm_format *fmt, uint64_t modifier);
bool wlr_drm_format_add(struct wlr_drm_format *fmt, uint64_t modifier);
bool wlr_drm_format_copy(struct wlr_drm_format *dst, const struct wlr_drm_format *src);
/**
 * Intersect modifiers for two DRM formats. The `dst` must be zeroed or initialized
 * with other state being replaced.
 *
 * Both arguments must have the same format field. If the formats aren't
 * compatible, NULL is returned. If either format doesn't support any modifier,
 * a format that doesn't support any modifier is returned.
 */
bool wlr_drm_format_intersect(struct wlr_drm_format *dst,
	const struct wlr_drm_format *a, const struct wlr_drm_format *b);

bool wlr_drm_format_set_copy(struct wlr_drm_format_set *dst, const struct wlr_drm_format_set *src);

#endif
