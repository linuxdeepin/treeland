#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput in_color;

layout(set = 1, binding = 0) uniform sampler3D lut_3d;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

/* struct wlr_vk_frag_output_pcr_data */
layout(push_constant) uniform UBO {
	layout(offset = 80) float lut_3d_offset;
	float lut_3d_scale;
} data;

layout (constant_id = 0) const int OUTPUT_TRANSFORM = 0;

// Matches enum wlr_vk_output_transform
#define OUTPUT_TRANSFORM_INVERSE_SRGB 0
#define OUTPUT_TRANSFORM_LUT_3D 1

float linear_channel_to_srgb(float x) {
	return max(min(x * 12.92, 0.04045), 1.055 * pow(x, 1. / 2.4) - 0.055);
}

vec3 linear_color_to_srgb(vec3 color) {
	return vec3(
		linear_channel_to_srgb(color.r),
		linear_channel_to_srgb(color.g),
		linear_channel_to_srgb(color.b)
	);
}

void main() {
	vec4 in_color = subpassLoad(in_color).rgba;

	// Convert from pre-multiplied alpha to straight alpha
	float alpha = in_color.a;
	vec3 rgb;
	if (alpha == 0) {
		rgb = vec3(0);
	} else {
		rgb = in_color.rgb / alpha;
	}

	if (OUTPUT_TRANSFORM == OUTPUT_TRANSFORM_LUT_3D) {
		// Apply 3D LUT
		vec3 pos = data.lut_3d_offset + rgb * data.lut_3d_scale;
		rgb = texture(lut_3d, pos).rgb;
	} else { // OUTPUT_TRANSFORM_INVERSE_SRGB
		// Produce sRGB encoded values
		rgb = linear_color_to_srgb(rgb);
	}

	// Back to pre-multiplied alpha
	out_color = vec4(rgb * alpha, alpha);
}
