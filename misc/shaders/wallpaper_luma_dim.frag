// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Lockscreen Luma-Adaptive Dimming (reverse-engineered empirical fit)
// - Derives the perceptual luma Y with the BT.709 weights (0.2126, 0.7152, 0.0722).
// - Scales all three RGB channels by (1 - u_dimRatio * Y), which keeps the RGB
//   ratio (and therefore hue/saturation) exactly intact while dimming highlights.
// - u_strength blends between the original image (0.0, desktop) and the dimmed
//   image (1.0, greeter), replacing the previous opacity-based fade-in.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float u_dimRatio; // Maximum dimming ratio applied to pure white
    float u_strength;      // Effect blend strength, animated 0.0 (desktop) ↔ 1.0 (greeter)
} ubuf;

layout(binding = 1) uniform sampler2D source;

// Scale all RGB channels by (1 - ratio * Y) using BT.709 luma, preserving the
// exact RGB ratio so hue and saturation stay untouched. The ratio is clamped to
// [0, 1] so an out-of-range uniform cannot make the factor negative on bright
// pixels (inverting their RGB) or brighten the wallpaper instead of dimming it.
vec3 apply_luma_adaptive_dim(vec3 color) {
    float ratio = clamp(ubuf.u_dimRatio, 0.0, 1.0);
    float y = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float factor = 1.0 - ratio * y;
    return color * factor;
}

void main() {
    vec4 texColor = texture(source, qt_TexCoord0);
    vec3 baseColor = texColor.rgb;

    vec3 processedColor = apply_luma_adaptive_dim(baseColor);
    vec3 finalColor = mix(baseColor, processedColor, ubuf.u_strength);

    fragColor = vec4(finalColor, texColor.a) * ubuf.qt_Opacity;
}
