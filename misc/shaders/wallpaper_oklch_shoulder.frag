// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// OKLCH Perceptual Shoulder Compression
// Operates in the perceptually uniform Oklab space:
//   - Keeps the hue angle completely intact (a, b unchanged → h unchanged)
//   - Filters precisely by perceptual chroma C: darkens near-white/gray regions,
//     while highly saturated pure colors are 100% immune
//   - Uses a C0/C1-continuous rational shoulder curve for monotonic compression,
//     producing a seamless gradient with no banding
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float threshold;    // Compression onset (lightness L), default 0.80
    float maxLightness; // Pure-white mapping ceiling, default 0.88
    float chromaStart;  // OKLCH chroma where filtering begins, default 0.02
    float chromaEnd;    // OKLCH chroma where colors become fully immune, default 0.08
} ubuf;

layout(binding = 1) uniform sampler2D source;

// ============================================================================
// 1. Color space conversion
// ============================================================================

// sRGB → Linear RGB (IEC 61966-2-1)
vec3 srgb_to_linear(vec3 c) {
    bvec3 cutoff = lessThanEqual(c, vec3(0.04045));
    vec3 higher = pow((c + vec3(0.055)) / vec3(1.055), vec3(2.4));
    vec3 lower = c / vec3(12.92);
    return mix(higher, lower, cutoff);
}

// Linear RGB → sRGB (IEC 61966-2-1)
vec3 linear_to_srgb(vec3 c) {
    c = max(c, vec3(0.0));
    bvec3 cutoff = lessThanEqual(c, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(c, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = c * vec3(12.92);
    return mix(higher, lower, cutoff);
}

// Linear RGB → Oklab (Björn Ottosson, 2020)
vec3 linear_to_oklab(vec3 c) {
    float l = 0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b;
    float m = 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b;
    float s = 0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b;

    float l_ = pow(max(l, 0.0), 1.0 / 3.0);
    float m_ = pow(max(m, 0.0), 1.0 / 3.0);
    float s_ = pow(max(s, 0.0), 1.0 / 3.0);

    float L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_;
    float a = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_;
    float b = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_;

    return vec3(L, a, b);
}

// Oklab → Linear RGB
vec3 oklab_to_linear(vec3 lab) {
    float l_ = lab.x + 0.3963377774 * lab.y + 0.2158037573 * lab.z;
    float m_ = lab.x - 0.1055613458 * lab.y - 0.0638541728 * lab.z;
    float s_ = lab.x - 0.0894841775 * lab.y - 1.2914855480 * lab.z;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    float r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    float g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    float b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    return vec3(r, g, b);
}

// ============================================================================
// 2. Highlight shoulder compression curve (C0 & C1 continuous rational compression)
// ============================================================================
// Monotonically compresses [x0, 1.0] into [x0, M]:
//   f(x) = x                                     for x <= x0
//   f(x) = x0 + (x - x0) / (1 + k * (x - x0))    for x > x0
// At x = x0: f(x0) = x0, f'(x0) = 1.0 (no inflection, no Mach banding)
// At x = 1.0: f(1.0) = M
float highlight_shoulder(float x, float x0, float M) {
    if (x <= x0) return x;
    float dx = 1.0 - x0;
    float dy = M - x0;
    float k = max((dx / dy - 1.0) / dx, 0.0);
    float diff = x - x0;
    return x0 + diff / (1.0 + k * diff);
}

// ============================================================================
// 3. Mode 4: OKLCH perceptual shoulder compression
// ============================================================================
// - Operates in the perceptually uniform Oklab space, keeping (a, b) unchanged
//   so the hue angle h stays intact.
// - Compresses lightness L only when perceptual chroma C is near 0 (near-white/gray).
// - Highly saturated pure yellows/greens/reds are safely ignored thanks to their large C.
vec3 apply_oklch_shoulder(vec3 color) {
    vec3 lrgb = srgb_to_linear(color);
    vec3 lab = linear_to_oklab(lrgb);
    float L = lab.x;
    float a = lab.y;
    float b = lab.z;

    float C = length(lab.yz);

    // Chroma mask: 1.0 for near-neutral colors, smoothly tapering to 0.0 as chroma rises
    float w_chroma = 1.0 - smoothstep(ubuf.chromaStart, ubuf.chromaEnd, C);

    // Lightness compression (shoulder curve)
    float L_comp = highlight_shoulder(L, ubuf.threshold, ubuf.maxLightness);
    float L_new = mix(L, L_comp, w_chroma);

    // Preserve hue and chroma: (a, b) kept as-is
    vec3 out_lab = vec3(L_new, a, b);
    vec3 out_lrgb = oklab_to_linear(out_lab);
    vec3 out_srgb = linear_to_srgb(out_lrgb);

    return clamp(out_srgb, 0.0, 1.0);
}

// ============================================================================
// 4. Main function
// ============================================================================
void main() {
    vec4 texColor = texture(source, qt_TexCoord0);
    vec3 baseColor = texColor.rgb;

    vec3 processedColor = apply_oklch_shoulder(baseColor);

    fragColor = vec4(processedColor, texColor.a) * ubuf.qt_Opacity;
}
