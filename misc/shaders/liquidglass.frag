// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

// Liquid Glass fragment shader — closely follows the liquid-dom reference
// implementation (https://github.com/AndrewPrifer/liquid-dom).
//
// Key concepts ported from liquid-dom's GLASS_SHADER (WGSL → GLSL 440):
//   • Convex-squircle height profile for the beveled edge
//   • Physical refraction via refract() with per-channel IOR dispersion
//   • Luminance-gated environment reflection
//   • Coloured edge specular (refracted ↔ reflected mix)
//   • Additive white rim specular with configurable opacity
//   • Tint, saturation, and brightness handled by MultiEffect (not shader)

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    float radius;             // corner radius (px)
    float bezelWidth;          // edge bevel width (px) — liquid-dom bezelWidth
    float thickness;           // base glass thickness (px) — liquid-dom thickness
    float displacementFactor;  // scalar on displacement — liquid-dom displacementFactor
    float ior;                 // refractive index — liquid-dom ior
    float dispersion;          // RGB channel separation — liquid-dom dispersion
    vec4 highlightColor;       // coloured specular tint (white in liquid-dom)
    float strokeWidth;         // specular rim band width (px) — liquid-dom specular.width
    float strokeStrength;      // specular strength — liquid-dom specular.strength
    vec2 lightDirection;       // 2D light dir (normalised at runtime)
    float lightPower;          // specular sharpness exponent — liquid-dom specular.sharpness
    float edgeSaturation;      // extra edge saturation boost
    float reflectionOffset;    // reflection sampling offset (px) — liquid-dom reflectionOffset
    float specularOpacity;     // white specular opacity — liquid-dom specular.opacity
    float rimReflectionStrength; // 0 = current pure-white rim, >0 tints rim from nearby backdrop
} ubuf;

layout(binding = 1) uniform sampler2D source;

// ---------------------------------------------------------------------------
// SDF helpers (rounded rectangle)
// ---------------------------------------------------------------------------

float roundedRectDistance(vec2 p, vec2 halfSize, float radius)
{
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 q = abs(p) - halfSize + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

vec2 roundedRectNormal(vec2 p, vec2 halfSize, float radius)
{
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 inner = halfSize - vec2(r);
    vec2 corner = clamp(p, -inner, inner);
    vec2 delta = p - corner;
    float len = length(delta);

    if (len > 0.0001)
        return delta / len;

    vec2 edge = halfSize - abs(p);
    if (edge.x < edge.y)
        return vec2(sign(p.x), 0.0);

    return vec2(0.0, sign(p.y));
}

// ---------------------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------------------

vec3 applySaturation(vec3 color, float saturationValue)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luminance), color, saturationValue);
}

vec4 sampleBackdrop(vec2 coord)
{
    return texture(source, clamp(coord, vec2(0.001), vec2(0.999)));
}

// ---------------------------------------------------------------------------
// Height profile: convex squircle (from liquid-dom)
// Returns vec2(height, derivative) where x is bezelProgress [0..1]:
//   x=0 at the shape boundary, x=1 at bezelWidth inside.
// The surface rises steeply at the edge and flattens toward the interior.
// ---------------------------------------------------------------------------

vec2 convexSquircle(float x)
{
    float u = 1.0 - clamp(x, 0.0, 1.0);
    float inside = max(1.0 - pow(u, 4.0), 0.0001);
    float height = sqrt(inside);
    float derivative = 2.0 * pow(u, 3.0) / sqrt(inside);
    return vec2(height, derivative);
}

// ---------------------------------------------------------------------------
// Fragment main
// ---------------------------------------------------------------------------

void main()
{
    vec2 size = max(ubuf.itemSize, vec2(1.0));
    vec2 pixelSize = 1.0 / size;
    vec2 pixel = texCoord * size;
    vec2 centered = pixel - size * 0.5;
    vec2 halfSize = size * 0.5;

    // SDF distance: negative inside, positive outside
    float sdf = roundedRectDistance(centered, halfSize, ubuf.radius);
    float distance = sdf;
    vec2 normal = roundedRectNormal(centered, halfSize, ubuf.radius);

    // Fill mask: 1 inside, 0 outside. Use SDF derivatives for antialiasing
    // so rounded corners stay smooth under scale/transform instead of using a
    // fixed feather or the specular rim width.
    float shapeAntialiasWidth = max(fwidth(sdf), 0.75);
    float fillMask = 1.0 - smoothstep(0.0, shapeAntialiasWidth, distance);

    // ── Bevel zone (convex squircle height profile) ──────────────────
    // Keep large bevels on straight edges, but avoid geometric singularities:
    // 1. tiny items cannot fit opposing bevel bands, so cap by item size;
    // 2. rounded corners cannot fit a bevel wider than the radius, so cap
    //    only inside the corner patch;
    // 3. square/overlapping corners blend x/y edge normals instead of jumping
    //    between the nearest horizontal/vertical edge normal.
    float configuredBezel = max(ubuf.bezelWidth, 1.0);
    float maxSizeBezel = max(
        min(configuredBezel, max(min(halfSize.x, halfSize.y) - shapeAntialiasWidth, 1.0)),
        1.0);
    float inwardDistance = max(-distance, 0.0);

    vec2 edgeDistance = max(halfSize - abs(centered), vec2(0.0));
    float xEdgeInfluence = 1.0 - smoothstep(0.0, maxSizeBezel, edgeDistance.x);
    float yEdgeInfluence = 1.0 - smoothstep(0.0, maxSizeBezel, edgeDistance.y);
    vec2 edgeSign = sign(centered);
    if (abs(edgeSign.x) < 0.0001)
        edgeSign.x = 1.0;
    if (abs(edgeSign.y) < 0.0001)
        edgeSign.y = 1.0;
    if (xEdgeInfluence + yEdgeInfluence > 0.0001) {
        normal = normalize(vec2(
            edgeSign.x * xEdgeInfluence,
            edgeSign.y * yEdgeInfluence));
    }

    float cornerRadius = min(ubuf.radius, min(halfSize.x, halfSize.y));
    vec2 cornerStart = halfSize - vec2(cornerRadius);
    vec2 cornerDelta = max(abs(centered) - cornerStart, vec2(0.0));
    float cornerMask = cornerRadius > 0.0
        ? smoothstep(
            0.0,
            max(shapeAntialiasWidth, 1.0),
            min(cornerDelta.x, cornerDelta.y))
        : 0.0;
    float cornerBezel = cornerRadius > 0.0
        ? min(maxSizeBezel, max(cornerRadius - shapeAntialiasWidth, 1.0))
        : maxSizeBezel;
    float bw = mix(maxSizeBezel, cornerBezel, cornerMask);
    float bezelProgress = clamp(inwardDistance / bw, 0.0, 1.0);

    vec2 profileResult = convexSquircle(bezelProgress);
    float profileHeight = profileResult.x * bw;
    float flatHeight = convexSquircle(1.0).x * bw;
    // Surface is thicker at the bevel edge, flattens in the interior
    float surfaceHeight = ubuf.thickness
        + (inwardDistance > bw ? flatHeight : profileHeight);

    // Surface slope from analytical derivative, clamped to ~85°
    float surfaceDerivative = (inwardDistance > bw) ? 0.0 : profileResult.y;
    float clampedSlope = min(surfaceDerivative, 11.43); // tan(1.4835) ≈ 85°
    vec2 surfaceSlope = normal * clampedSlope;

    // 3D surface normal from 2D slope
    vec3 surfaceNormal = normalize(vec3(surfaceSlope, 1.0));

    // Shared light direction for both edge optics and specular rim.  The
    // physical view-ray refraction remains normal-based; this only modulates
    // the bezel's optical strength so internal dispersion and border highlight
    // read as one coherent light setup.
    vec2 rawLightDir = ubuf.lightDirection;
    vec2 lightDir = length(rawLightDir) > 0.0001
        ? normalize(rawLightDir)
        : vec2(-0.70710678, -0.70710678);
    float lightFacing = clamp(dot(normal, lightDir) * 0.5 + 0.5, 0.0, 1.0);
    float directionalOpticalStrength = mix(0.85, 1.15, lightFacing);

    // ── Physical refraction with per-channel IOR dispersion ───────────
    float baseIor = max(ubuf.ior, 1.0001);
    float disp = max(ubuf.dispersion, 0.0) * directionalOpticalStrength;

    vec3 refractedRayRed = refract(
        vec3(0.0, 0.0, -1.0), surfaceNormal,
        1.0 / max(baseIor + disp, 1.0001));
    vec3 refractedRayGreen = refract(
        vec3(0.0, 0.0, -1.0), surfaceNormal,
        1.0 / baseIor);
    vec3 refractedRayBlue = refract(
        vec3(0.0, 0.0, -1.0), surfaceNormal,
        1.0 / max(baseIor - disp, 1.0001));

    // Per-channel pixel displacement (zero outside the shape)
    float displaceScale = fillMask > 0.0 ? 1.0 : 0.0;
    vec2 displacementRed = refractedRayRed.xy
        / max(-refractedRayRed.z, 0.0001)
        * surfaceHeight * ubuf.displacementFactor * directionalOpticalStrength * displaceScale;
    vec2 displacementGreen = refractedRayGreen.xy
        / max(-refractedRayGreen.z, 0.0001)
        * surfaceHeight * ubuf.displacementFactor * directionalOpticalStrength * displaceScale;
    vec2 displacementBlue = refractedRayBlue.xy
        / max(-refractedRayBlue.z, 0.0001)
        * surfaceHeight * ubuf.displacementFactor * directionalOpticalStrength * displaceScale;

    vec2 refractedUvRed = texCoord + displacementRed * pixelSize;
    vec2 refractedUvGreen = texCoord + displacementGreen * pixelSize;
    vec2 refractedUvBlue = texCoord + displacementBlue * pixelSize;

    // Sample backdrop with refraction (liquid-dom uses blurred texture here)
    vec3 refractedColor = vec3(
        sampleBackdrop(refractedUvRed).r,
        sampleBackdrop(refractedUvGreen).g,
        sampleBackdrop(refractedUvBlue).b
    );

    // ── Environment reflection (luminance-gated) ──────────────────────
    vec2 reflectedUv = texCoord + normal * ubuf.reflectionOffset * pixelSize;
    vec3 reflectedColor = sampleBackdrop(reflectedUv).rgb;

    // Glass interior = refracted colour (no tint/exposure — MultiEffect handles those)
    vec3 glass = refractedColor;

    // Reflection only shows when reflected area is bright AND refracted area is dark
    float refractedLuma = dot(refractedColor, vec3(0.2126, 0.7152, 0.0722));
    float reflectedLuma = dot(reflectedColor, vec3(0.2126, 0.7152, 0.0722));
    float reflectionPresence = smoothstep(0.2, 0.85, reflectedLuma);
    float refractionAcceptance = 1.0 - smoothstep(0.35, 0.85, refractedLuma);
    float reflectionBlend = reflectionPresence * refractionAcceptance;
    vec3 edgeSpecularColor = mix(refractedColor, reflectedColor, reflectionBlend);

    // ── Edge-local saturation boost (global saturation is MultiEffect's job) ──
    // Glass material only affects the bezel zone. The flat interior shows
    // the original backdrop untouched — like a transparent window with
    // beveled edges.
    vec3 glassInterior = glass;
    float edgeInfluence = 1.0 - smoothstep(0.0, bw, inwardDistance);
    // edgeSaturation is an additive boost on top of MultiEffect's global saturation
    glassInterior = applySaturation(glassInterior, 1.0 + edgeInfluence * ubuf.edgeSaturation);

    // ── Specular rim highlights (directional, from liquid-dom) ───────
    float rimWidthPx = max(ubuf.strokeWidth, 0.0001);
    float specularInwardDistancePx = max(-distance, 0.0);
    float specularOuterMask = 1.0 - smoothstep(0.0, 1.0, max(distance, 0.0));
    float specularInnerMask = 1.0 - smoothstep(
        rimWidthPx, rimWidthPx + 1.0, specularInwardDistancePx);
    float rimBandMask = specularOuterMask * specularInnerMask;

    // Inward progress: 0 at boundary, 1 at rimWidth inside
    float inwardProgress = clamp(
        specularInwardDistancePx / max(rimWidthPx, 1.0), 0.0, 1.0);
    // Quadratic falloff (liquid-dom: specularFalloff * progress²)
    float strengthFalloff = 1.0 - inwardProgress * inwardProgress * 0.5;

    vec2 specularNormal = lightDir;
    vec2 rimReflectionUv = texCoord
        + specularNormal * ubuf.reflectionOffset * pixelSize;
    vec3 rimReflectionColor = sampleBackdrop(rimReflectionUv).rgb;
    vec3 rimReflectionTint = mix(
        vec3(1.0), rimReflectionColor, ubuf.rimReflectionStrength);
    float normalAlignment = dot(normal, specularNormal);

    // Primary and opposite rim contributions are both derived from the same
    // configurable specular normal, matching the reference "Specular Angle"
    // control: rotating one normal rotates the whole rim highlight pattern.
    float rimSpecular = pow(max(normalAlignment, 0.0), ubuf.lightPower);
    float oppositeRimSpecular = pow(max(-normalAlignment, 0.0), ubuf.lightPower);

    float primaryOpacity = clamp(
        rimSpecular * ubuf.strokeStrength * strengthFalloff, 0.0, 1.0);
    float oppositeOpacity = clamp(
        oppositeRimSpecular * ubuf.strokeStrength * strengthFalloff, 0.0, 1.0);
    float combinedSpecular = clamp(
        (primaryOpacity + oppositeOpacity) * rimBandMask, 0.0, 1.0);

    // White specular (additive, modulated by specularOpacity)
    float whiteSpecularOpacity = combinedSpecular * ubuf.specularOpacity;
    vec3 whiteSpecular = rimReflectionTint * whiteSpecularOpacity;

    // Coloured specular tint (highlightColor, for compatibility with
    // liquid-dom's pure-white specular we default highlightColor to white)
    vec3 coloredSpecular = ubuf.highlightColor.rgb
        * ubuf.highlightColor.a
        * combinedSpecular * 0.5;

    // ── Compositing (from liquid-dom) ─────────────────────────────────
    vec3 background = sampleBackdrop(texCoord).rgb;
    vec3 color = background;
    if (fillMask > 0.0) {
        // 1. Glass interior (refracted + edge saturation boost)
        //    Limited to the bezel edge zone; center is transparent.
        color = mix(color, glassInterior, edgeInfluence * fillMask);
        // 2. Coloured edge specular (refracted ↔ reflected mix) — edge only
        color = mix(color, edgeSpecularColor, combinedSpecular * edgeInfluence * fillMask);
        // 3. White specular (additive) — rim band only
        color = color + whiteSpecular * fillMask;
        // 4. Coloured specular tint — rim band only
        color = color + coloredSpecular * fillMask;
    }

    // ── Corner alpha (rounded-rect clip, SDF derivative antialiasing) ──
    float cornerAlpha = 1.0 - smoothstep(0.0, shapeAntialiasWidth, max(sdf, 0.0));
    // Premultiplied alpha: transparent pixels must have zero RGB too,
    // otherwise MultiEffect's blur bleeds false colour (white corners)
    // when sampling the layer texture.
    float outAlpha = cornerAlpha * ubuf.qt_Opacity;
    fragColor = vec4(clamp(color, vec3(0.0), vec3(1.0)) * outAlpha, outAlpha);
}
