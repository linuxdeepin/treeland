#include <lcms2.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/render/color.h>
#include "render/color.h"

static const cmsCIExyY srgb_whitepoint = { 0.3127, 0.3291, 1 };

static const cmsCIExyYTRIPLE srgb_primaries = {
	.Red = { 0.64, 0.33, 1 },
	.Green = { 0.3, 0.6, 1 },
	.Blue = { 0.15, 0.06, 1},
};

static void handle_lcms_error(cmsContext ctx, cmsUInt32Number code, const char *text) {
	wlr_log(WLR_ERROR, "[lcms] %s", text);
}

struct wlr_color_transform *wlr_color_transform_init_linear_to_icc(
		const void *data, size_t size) {
	struct wlr_color_transform_lut3d *tx = NULL;

	cmsContext ctx = cmsCreateContext(NULL, NULL);
	if (ctx == NULL) {
		wlr_log(WLR_ERROR, "cmsCreateContext failed");
		return NULL;
	}

	cmsSetLogErrorHandlerTHR(ctx, handle_lcms_error);

	cmsHPROFILE icc_profile = cmsOpenProfileFromMemTHR(ctx, data, size);
	if (icc_profile == NULL) {
		wlr_log(WLR_ERROR, "cmsOpenProfileFromMemTHR failed");
		goto out_ctx;
	}

	if (cmsGetDeviceClass(icc_profile) != cmsSigDisplayClass) {
		wlr_log(WLR_ERROR, "ICC profile must have the Display device class");
		goto out_icc_profile;
	}

	cmsToneCurve *linear_tone_curve = cmsBuildGamma(ctx, 1);
	if (linear_tone_curve == NULL) {
		wlr_log(WLR_ERROR, "cmsBuildGamma failed");
		goto out_icc_profile;
	}

	cmsToneCurve *linear_tf[] = {
		linear_tone_curve,
		linear_tone_curve,
		linear_tone_curve,
	};
	cmsHPROFILE srgb_profile = cmsCreateRGBProfileTHR(ctx, &srgb_whitepoint,
		&srgb_primaries, linear_tf);
	if (srgb_profile == NULL) {
		wlr_log(WLR_ERROR, "cmsCreateRGBProfileTHR failed");
		goto out_linear_tone_curve;
	}

	cmsHTRANSFORM lcms_tr = cmsCreateTransformTHR(ctx,
		srgb_profile, TYPE_RGB_FLT, icc_profile, TYPE_RGB_FLT,
		INTENT_RELATIVE_COLORIMETRIC, 0);
	if (lcms_tr == NULL) {
		wlr_log(WLR_ERROR, "cmsCreateTransformTHR failed");
		goto out_srgb_profile;
	}

	size_t dim_len = 33;
	float *lut_3d = calloc(3 * dim_len * dim_len * dim_len, sizeof(float));
	if (lut_3d == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		goto out_lcms_tr;
	}

	float factor = 1.0f / (dim_len - 1);
	for (size_t b_index = 0; b_index < dim_len; b_index++) {
		for (size_t g_index = 0; g_index < dim_len; g_index++) {
			for (size_t r_index = 0; r_index < dim_len; r_index++) {
				float rgb_in[3] = {
					r_index * factor,
					g_index * factor,
					b_index * factor,
				};
				float rgb_out[3];
				// TODO: use a single call to cmsDoTransform for the entire calculation?
				// this does require allocating an extra temp buffer
				cmsDoTransform(lcms_tr, rgb_in, rgb_out, 1);

				size_t offset = 3 * (r_index + dim_len * g_index + dim_len * dim_len * b_index);
				// TODO: maybe clamp values to [0.0, 1.0] here?
				lut_3d[offset] = rgb_out[0];
				lut_3d[offset + 1] = rgb_out[1];
				lut_3d[offset + 2] = rgb_out[2];
			}
		}
	}

	tx = calloc(1, sizeof(struct wlr_color_transform_lut3d));
	if (!tx) {
		goto out_lcms_tr;
	}
	tx->base.type = COLOR_TRANSFORM_LUT_3D;
	tx->dim_len = dim_len;
	tx->lut_3d = lut_3d;
	tx->base.ref_count = 1;
	wlr_addon_set_init(&tx->base.addons);

out_lcms_tr:
	cmsDeleteTransform(lcms_tr);
out_linear_tone_curve:
	cmsFreeToneCurve(linear_tone_curve);
out_srgb_profile:
	cmsCloseProfile(srgb_profile);
out_icc_profile:
	cmsCloseProfile(icc_profile);
out_ctx:
	cmsDeleteContext(ctx);
	return &tx->base;
}
