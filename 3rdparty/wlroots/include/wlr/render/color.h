/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_COLOR_H
#define WLR_RENDER_COLOR_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * Well-known color primaries.
 */
enum wlr_color_named_primaries {
	WLR_COLOR_NAMED_PRIMARIES_SRGB = 1 << 0,
	WLR_COLOR_NAMED_PRIMARIES_BT2020 = 1 << 1,
};

/**
 * Well-known color transfer functions.
 */
enum wlr_color_transfer_function {
	WLR_COLOR_TRANSFER_FUNCTION_SRGB = 1 << 0,
	WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ = 1 << 1,
};

/**
 * CIE 1931 xy chromaticity coordinates.
 */
struct wlr_color_cie1931_xy {
	float x, y;
};

/**
 * Color primaries and white point describing a color volume.
 */
struct wlr_color_primaries {
	struct wlr_color_cie1931_xy red, green, blue, white;
};

/**
 * Luminance range and reference white luminance level, in cd/m².
 */
struct wlr_color_luminances {
	float min, max, reference;
};

/**
 * A color transformation formula, which maps a linear color space with
 * sRGB primaries to an output color space.
 *
 * For ease of use, this type is heap allocated and reference counted.
 * Use wlr_color_transform_ref()/wlr_color_transform_unref(). The initial reference
 * count after creation is 1.
 *
 * Color transforms are immutable; their type/parameters should not be changed,
 * and this API provides no functions to modify them after creation.
 *
 * This formula may be implemented using a 3d look-up table, or some other
 * means.
 */
struct wlr_color_transform;

/**
 * Initialize a color transformation to convert linear
 * (with sRGB(?) primaries) to an ICC profile. Returns NULL on failure.
 */
struct wlr_color_transform *wlr_color_transform_init_linear_to_icc(
	const void *data, size_t size);

/**
 * Initialize a color transformation to apply sRGB encoding.
 * Returns NULL on failure.
 */
struct wlr_color_transform *wlr_color_transform_init_srgb(void);

/**
 * Increase the reference count of the color transform by 1.
 */
struct wlr_color_transform *wlr_color_transform_ref(struct wlr_color_transform *tr);

/**
 * Reduce the reference count of the color transform by 1; freeing it and
 * all associated resources when the reference count hits zero.
 */
void wlr_color_transform_unref(struct wlr_color_transform *tr);

#endif
