// SPDX-FileCopyrightText: 2017-2023 wlroots contributors
// SPDX-License-Identifier: MIT
//
// Vendored from wlroots 0.19.3 (commit 88a869855742281c98c22cab9641b317b8d065ef)
// Source path: render/color_fallback.c
// Modifications: Vendored for waylib sgrenderer. Initial vendor, no functional changes.

#include <wlr/render/color.h>
#include <wlr/util/log.h>

struct wlr_color_transform *wlr_color_transform_init_linear_to_icc(
		const void *data, size_t size) {
	wlr_log(WLR_ERROR, "Cannot create color transform from ICC profile: "
		"LCMS2 is compile-time disabled");
	return NULL;
}
