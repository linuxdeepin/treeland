#include <wlr/render/color.h>
#include <wlr/util/log.h>

struct wlr_color_transform *wlr_color_transform_init_linear_to_icc(
		const void *data, size_t size) {
	wlr_log(WLR_ERROR, "Cannot create color transform from ICC profile: "
		"LCMS2 is compile-time disabled");
	return NULL;
}
