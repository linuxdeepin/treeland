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
    float height = pow(inside, 0.25);
    float derivative = pow(u, 3.0) / pow(inside, 0.75);
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

    // Distance from the SDF boundary drives the bevel profile.  The refraction
    // direction itself stays on the rounded-rect SDF gradient; overriding it
    // with axis-aligned edge normals breaks the continuous displacement vector
    // field used by liquid-dom and SVG displacement-map implementations.
    float bw = maxSizeBezel;

    float bezelProgress = clamp(inwardDistance / bw, 0.0, 1.0);

    vec2 profileResult = convexSquircle(bezelProgress);
    float profileHeight = profileResult.x * bw;
    float flatHeight = convexSquircle(1.0).x * bw;
    // Surface is thicker at the bevel edge, flattens in the interior.
    float surfaceHeight = ubuf.thickness
        + (inwardDistance > bw ? flatHeight : profileHeight);

    // ── Refraction Slope (Geometric Radiating Field) ─────────────────────────
    // The user's hand-drawn sketch explicitly defines the displacement vector field:
    // 1. Vectors are straight lines (no curving along their path).
    // 2. On straight edges, vectors are perfectly perpendicular.
    // 3. In corners, vectors radiate from a deep inner center.
    // 4. The fanning region starts exactly `bw` pixels earlier than the outer corner.
    // 
    // By calculating the normal using an expanded radius `ubuf.radius + bw`, we
    // push the center of the normal fan inward by `bw`. This perfectly recreates
    // the vector field shown in the sketch, extending the corner's diagonal pull
    // exactly `bw` pixels along the straight edges!
    vec2 refractionNormal = roundedRectNormal(centered, halfSize, ubuf.radius + bw);

    // Surface slope from analytical derivative, clamped to ~85°.
    float surfaceDerivative = (inwardDistance > bw) ? 0.0 : profileResult.y;
    float clampedSlope = min(surfaceDerivative, 11.43); // tan(1.4835) ≈ 85°
    vec2 surfaceSlope = refractionNormal * clampedSlope;

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
    
    // Prism Effect (Caustics/Curvature): Dispersion is violently amplified at the 
    // steep edges where the surface normal tilts away from the viewer (Fresnel-like).
    // This simulates the complex lens focusing that creates bright, separated color bands.
    float prismEffect = 1.0 + pow(1.0 - surfaceNormal.z, 2.0) * 0.3;
    float disp = max(ubuf.dispersion, 0.0) * directionalOpticalStrength * prismEffect;

    vec3 refractedRayRed = refract(
        vec3(0.0, 0.0, -1.0), surfaceNormal,
        1.0 / max(baseIor - disp, 1.0001));
    vec3 refractedRayGreen = refract(
        vec3(0.0, 0.0, -1.0), surfaceNormal,
        1.0 / baseIor);
    vec3 refractedRayBlue = refract(
        vec3(0.0, 0.0, -1.0), surfaceNormal,
        1.0 / max(baseIor + disp, 1.0001));

    // Per-channel pixel displacement (zero outside the shape).
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

    vec2 refractedUvRed   = texCoord + displacementRed   * pixelSize;
    vec2 refractedUvGreen = texCoord + displacementGreen * pixelSize;
    vec2 refractedUvBlue  = texCoord + displacementBlue  * pixelSize;

    // Sample backdrop with refraction. Bilinear filtering smoothly blends 
    // the small sub-pixel RGB splits into continuous cyan/red fringes.
    vec3 refractedColor = vec3(
        sampleBackdrop(refractedUvRed).r,
        sampleBackdrop(refractedUvGreen).g,
        sampleBackdrop(refractedUvBlue).b
    );

    float edgeInfluence = 1.0 - smoothstep(0.0, bw, inwardDistance);

    // ── Environment reflection (luminance-gated with internal dispersion) ──
    // Edge reflections in thick curved glass exhibit color separation due to 
    // multiple internal reflections and high curvature. We simulate this by 
    // applying a scaled dispersion to the reflection offset.
    float refDisp = max(ubuf.dispersion, 0.0) * 2.0; 
    vec2 reflectedUvRed   = texCoord + normal * (ubuf.reflectionOffset * (1.0 - refDisp)) * pixelSize;
    vec2 reflectedUvGreen = texCoord + normal * ubuf.reflectionOffset * pixelSize;
    vec2 reflectedUvBlue  = texCoord + normal * (ubuf.reflectionOffset * (1.0 + refDisp)) * pixelSize;

    vec3 reflectedColor = vec3(
        sampleBackdrop(reflectedUvRed).r,
        sampleBackdrop(reflectedUvGreen).g,
        sampleBackdrop(reflectedUvBlue).b
    );

    vec3 glass = refractedColor;

    // Reflection shows when reflected area is bright AND refracted area is dark.
    // We add a Fresnel term so the reflection naturally strengthens at steep edges.
    float refractedLuma = dot(refractedColor, vec3(0.2126, 0.7152, 0.0722));
    float reflectedLuma = dot(reflectedColor, vec3(0.2126, 0.7152, 0.0722));
    float reflectionPresence = smoothstep(0.2, 0.85, reflectedLuma);
    float refractionAcceptance = 1.0 - smoothstep(0.35, 0.85, refractedLuma);
    
    float fresnel = pow(1.0 - surfaceNormal.z, 3.0);
    float reflectionBlend = clamp(reflectionPresence * refractionAcceptance + fresnel * 0.3, 0.0, 1.0);

    vec3 edgeSpecularColor = mix(refractedColor, reflectedColor, reflectionBlend);

    // ── Edge-local saturation boost (global saturation is MultiEffect's job) ──
    // Glass material only affects the bezel zone. The flat interior shows
    // the original backdrop untouched — like a transparent window with
    // beveled edges.
    vec3 glassInterior = glass;
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
    
    // Reuse the dispersed reflectedColor to tint the specular rim, ensuring
    // the white highlights also gain the beautiful rainbow chromatic aberration.
    vec3 rimReflectionTint = mix(
        vec3(1.0), reflectedColor, ubuf.rimReflectionStrength);
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
        // 1. Glass interior: the whole shape composites the same refracted
        //    surface field. The center stays visually transparent when the
        //    source is unblurred because refractedColor equals background
        //    over the flat interior.
        color = mix(color, glassInterior, fillMask);
        // 2. Coloured edge specular (refracted ↔ reflected mix)
        color = mix(color, edgeSpecularColor, combinedSpecular * fillMask);
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
