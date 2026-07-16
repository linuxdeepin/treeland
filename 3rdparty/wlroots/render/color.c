#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/color.h>
#include "render/color.h"
#include "util/matrix.h"

// See H.273 ColourPrimaries

static const struct wlr_color_primaries COLOR_PRIMARIES_SRGB = { // code point 1
	.red = { 0.640, 0.330 },
	.green = { 0.300, 0.600 },
	.blue = { 0.150, 0.060 },
	.white = { 0.3127, 0.3290 },
};

static const struct wlr_color_primaries COLOR_PRIMARIES_BT2020 = { // code point 9
	.red = { 0.708, 0.292 },
	.green = { 0.170, 0.797 },
	.blue = { 0.131, 0.046 },
	.white = { 0.3127, 0.3290 },
};

struct wlr_color_transform *wlr_color_transform_init_srgb(void) {
	struct wlr_color_transform *tx = calloc(1, sizeof(struct wlr_color_transform));
	if (!tx) {
		return NULL;
	}
	tx->type = COLOR_TRANSFORM_SRGB;
	tx->ref_count = 1;
	wlr_addon_set_init(&tx->addons);
	return tx;
}

static void color_transform_destroy(struct wlr_color_transform *tr) {
	switch (tr->type) {
	case COLOR_TRANSFORM_SRGB:
		break;
	case COLOR_TRANSFORM_LUT_3D:;
		struct wlr_color_transform_lut3d *lut3d =
			wlr_color_transform_lut3d_from_base(tr);
		free(lut3d->lut_3d);
		break;
	}
	wlr_addon_set_finish(&tr->addons);
	free(tr);
}

struct wlr_color_transform *wlr_color_transform_ref(struct wlr_color_transform *tr) {
	tr->ref_count += 1;
	return tr;
}

void wlr_color_transform_unref(struct wlr_color_transform *tr) {
	if (!tr) {
		return;
	}
	assert(tr->ref_count > 0);
	tr->ref_count -= 1;
	if (tr->ref_count == 0) {
		color_transform_destroy(tr);
	}
}

struct wlr_color_transform_lut3d *wlr_color_transform_lut3d_from_base(
		struct wlr_color_transform *tr) {
	assert(tr->type == COLOR_TRANSFORM_LUT_3D);
	struct wlr_color_transform_lut3d *lut3d = wl_container_of(tr, lut3d, base);
	return lut3d;
}

void wlr_color_primaries_from_named(struct wlr_color_primaries *out,
		enum wlr_color_named_primaries named) {
	switch (named) {
	case WLR_COLOR_NAMED_PRIMARIES_SRGB:
		*out = COLOR_PRIMARIES_SRGB;
		return;
	case WLR_COLOR_NAMED_PRIMARIES_BT2020:
		*out = COLOR_PRIMARIES_BT2020;
		return;
	}
	abort();
}

static void multiply_matrix_vector(float out[static 3], float m[static 9], float v[static 3]) {
	float result[3] = {
		m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
		m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
		m[6] * v[0] + m[7] * v[1] + m[8] * v[2],
	};
	memcpy(out, result, sizeof(result));
}

static void xy_to_xyz(float out[static 3], struct wlr_color_cie1931_xy src) {
	if (src.y == 0) {
		out[0] = out[1] = out[2] = 0;
		return;
	}

	out[0] = src.x / src.y;
	out[1] = 1;
	out[2] = (1 - src.x - src.y) / src.y;
}

void wlr_color_primaries_to_xyz(const struct wlr_color_primaries *primaries, float matrix[static 9]) {
	// See: http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html

	float r[3], g[3], b[3], w[3];
	xy_to_xyz(r, primaries->red);
	xy_to_xyz(g, primaries->green);
	xy_to_xyz(b, primaries->blue);
	xy_to_xyz(w, primaries->white);

	float xyz_matrix[9] = {
		r[0], g[0], b[0],
		r[1], g[1], b[1],
		r[2], g[2], b[2],
	};
	matrix_invert(xyz_matrix, xyz_matrix);

	float S[3];
	multiply_matrix_vector(S, xyz_matrix, w);

	float result[] = {
		S[0] * r[0], S[1] * g[0], S[2] * b[0],
		S[0] * r[1], S[1] * g[1], S[2] * b[1],
		S[0] * r[2], S[1] * g[2], S[2] * b[2],
	};
	memcpy(matrix, result, sizeof(result));
}

void wlr_color_transfer_function_get_default_luminance(enum wlr_color_transfer_function tf,
		struct wlr_color_luminances *lum) {
	switch (tf) {
	case WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ:
		*lum = (struct wlr_color_luminances){
			.min = 0.005,
			.max = 10000,
			.reference = 203,
		};
		break;
	default:
		*lum = (struct wlr_color_luminances){
			.min = 0.2,
			.max = 80,
			.reference = 80,
		};
		break;
	}
}
