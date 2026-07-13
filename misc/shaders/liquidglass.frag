// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

// Liquid Glass fragment shader — closely follows the liquid-dom reference
// implementation (https://github.com/AndrewPrifer/liquid-dom).
//
// Key concepts ported from liquid-dom's GLASS_SHADER (WGSL → GLSL 440):
//   • Convex-squircle height profile for the beveled edge
//   • Physical refraction via refract() with per-channel IOR dispersion
//   • Luminance-gated environment reflection (rim band only)
//   • Coloured edge specular (refracted ↔ reflected mix)
//   • Additive white rim specular with configurable opacity
//   • Tint, saturation, and brightness handled by MultiEffect (not shader)
//
// Performance structure (hot path, sample-bound):
//   1. Outside AA band          → 0 samples
//   2. Flat interior            → 1 sample
//   3. Bezel interior           → 1–3 sharp refraction samples
//   4. Silhouette AA band only  → 2×2 mono geometric SS
//   5. Lit rim                  → + reflection samples
//   Dispersion RGB split is gated on the uniform (coherent across the draw).
//   Do NOT multi-tap filter warped content: it smears bright icon edges into
//   pale ghosts. Keep a single bilinear sample so ellipses stay sharp.

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
    vec2 lightDirection;       // 2D light dir (renormalized in shader)
    float lightPower;          // specular sharpness exponent — liquid-dom specular.sharpness
    float edgeSaturation;      // extra edge saturation boost
    float reflectionOffset;    // reflection sampling offset (px) — liquid-dom reflectionOffset
    float specularOpacity;     // white specular opacity — liquid-dom specular.opacity
    float rimReflectionStrength; // 0 = pure-white rim, >0 tints rim from backdrop
} ubuf;

layout(binding = 1) uniform sampler2D source;

const vec3 kLumaWeights = vec3(0.2126, 0.7152, 0.0722);
const vec3 kViewDir = vec3(0.0, 0.0, -1.0);

// ---------------------------------------------------------------------------
// SDF helpers (rounded rectangle)
// ---------------------------------------------------------------------------

// Returns (signed distance, outward surface normal).
// Flat-edge midpoints use an axis-aligned fallback when delta ~ 0.
vec3 roundedRectSDF(vec2 p, vec2 halfSize, float radius)
{
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 inner = halfSize - vec2(r);
    vec2 delta = p - clamp(p, -inner, inner);
    float len2 = dot(delta, delta);
    // inversesqrt avoids a separate sqrt+divide for the normal.
    float invLen = inversesqrt(max(len2, 1e-16));
    float len = len2 * invLen; // == sqrt(len2) for len2 > 0

    // SDF: distance to the rounded rectangle boundary (negative inside).
    vec2 q = abs(p) - inner;
    float dist = len + min(max(q.x, q.y), 0.0) - r;

    vec2 edge = halfSize - abs(p);
    float useX = step(edge.x, edge.y);
    vec2 axisNormal = vec2(sign(p.x) * useX, sign(p.y) * (1.0 - useX));
    vec2 normal = (len2 > 1e-16) ? (delta * invLen) : axisNormal;

    return vec3(dist, normal);
}

// Normal-only path for the expanded-radius refraction field (dist unused).
vec2 roundedRectNormal(vec2 p, vec2 halfSize, float radius)
{
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 inner = halfSize - vec2(r);
    vec2 delta = p - clamp(p, -inner, inner);
    float len2 = dot(delta, delta);
    if (len2 > 1e-16)
        return delta * inversesqrt(len2);

    vec2 edge = halfSize - abs(p);
    float useX = step(edge.x, edge.y);
    return vec2(sign(p.x) * useX, sign(p.y) * (1.0 - useX));
}

// ---------------------------------------------------------------------------
// Colour / math helpers
// ---------------------------------------------------------------------------

vec3 applySaturation(vec3 color, float saturationValue)
{
    float luminance = dot(color, kLumaWeights);
    return mix(vec3(luminance), color, saturationValue);
}

vec4 sampleBackdrop(vec2 coord)
{
    // Soft clamp: ShaderEffect source wrap is not guaranteed clamp-to-edge.
    return texture(source, clamp(coord, vec2(0.001), vec2(0.999)));
}

// Product defaults are lightPower 2 or 3 — multiply is cheaper than pow.
float specularPower(float x, float p)
{
    float v = max(x, 0.0);
    // Uniform-driven: whole draw takes one arm, no pixel divergence.
    if (abs(p - 2.0) < 0.001)
        return v * v;
    if (abs(p - 3.0) < 0.001)
        return v * v * v;
    if (abs(p - 4.0) < 0.001) {
        float v2 = v * v;
        return v2 * v2;
    }
    return pow(v, p);
}

// Height profile: convex squircle (liquid-dom).
// Returns vec2(height, derivative); x is bezelProgress [0..1]
// (0 = shape boundary, 1 = bezelWidth inside).
// height  = (1 - u^4)^0.25  where u = 1 - clamp(x, 0, 1)
// derivative = d(height)/dx = u^3 * height / inside  [= u^3 / inside^0.75]
vec2 convexSquircle(float x)
{
    float u = 1.0 - clamp(x, 0.0, 1.0);
    float u2 = u * u;
    float u4 = u2 * u2;
    float inside = max(1.0 - u4, 0.0001);
    float height = sqrt(sqrt(inside));                  // inside^0.25
    float derivative = (u2 * u) * height / inside;     // u^3 / inside^0.75
    return vec2(height, derivative);
}

vec2 refractDisplacement(vec3 ray, float scale)
{
    return ray.xy / max(-ray.z, 0.0001) * scale;
}

// Sample backdrop with optional RGB dispersion along `offsetDir` (in UV).
// Uniform `useDispersion` keeps the branch coherent across the whole draw.
vec3 sampleDispersed(vec2 baseUv, vec2 offsetDir, float offsetPx, vec2 pixelSize, bool useDispersion)
{
    vec2 baseOffset = offsetDir * (offsetPx * pixelSize);
    if (useDispersion) {
        float refDisp = max(ubuf.dispersion, 0.0) * 2.0;
        vec2 uvR = baseUv + baseOffset * (1.0 - refDisp);
        vec2 uvG = baseUv + baseOffset;
        vec2 uvB = baseUv + baseOffset * (1.0 + refDisp);
        return vec3(
            sampleBackdrop(uvR).r,
            sampleBackdrop(uvG).g,
            sampleBackdrop(uvB).b
        );
    }
    return sampleBackdrop(baseUv + baseOffset).rgb;
}

// Refraction at pixel-space point `p` (origin = item center). Used for
// silhouette 2×2 supersampling where the normal rotates within one pixel.
// `aaWidth` is the parent fragment's screen-space SDF width (fwidth at offsets
// is unreliable). Edge SS always uses mono IOR; dispersion stays on the
// single-center bezel path.
vec3 sampleRefractedAt(
    vec2 p,
    vec2 halfSize,
    vec2 size,
    vec2 pixelSize,
    float bw,
    float aaWidth,
    float baseIor,
    vec2 lightDir)
{
    vec3 localSdf = roundedRectSDF(p, halfSize, ubuf.radius);
    float sdf = localSdf.x;
    vec2 n = localSdf.yz;
    float inward = max(-sdf, 0.0);
    float cover = 1.0 - smoothstep(0.0, aaWidth, sdf);
    vec2 uv = (p + halfSize) / size;

    if (cover <= 1e-4)
        return sampleBackdrop(uv).rgb;

    float bezelProgress = inward / bw;
    vec2 profile = convexSquircle(bezelProgress);
    float surfaceHeight = ubuf.thickness + profile.x * bw;
    vec2 refrN = roundedRectNormal(p, halfSize, ubuf.radius + bw);
    float bevelFade = 1.0 - smoothstep(bw * 0.85, bw, inward);
    float outerSoft = smoothstep(0.0, max(2.5 * aaWidth, 2.5), inward);
    float slope = min(profile.y * bevelFade * outerSoft, 8.0);
    vec3 surfN = normalize(vec3(refrN * slope, 1.0));

    float lightFacing = clamp(dot(n, lightDir) * 0.5 + 0.5, 0.0, 1.0);
    float dirOpt = mix(0.85, 1.15, lightFacing);
    float scale = surfaceHeight * ubuf.displacementFactor * dirOpt * cover;

    if (slope < 1e-4 || scale < 1e-4)
        return sampleBackdrop(uv).rgb;

    vec3 ray = refract(kViewDir, surfN, 1.0 / baseIor);
    // Geometric 2×2 already SS the silhouette; single bilinear is enough here.
    return sampleBackdrop(uv + refractDisplacement(ray, scale) * pixelSize).rgb;
}

// ---------------------------------------------------------------------------
// Fragment main
// ---------------------------------------------------------------------------

void main()
{
    vec2 size = max(ubuf.itemSize, vec2(1.0));
    vec2 pixelSize = 1.0 / size;
    vec2 centered = texCoord * size - size * 0.5;
    vec2 halfSize = size * 0.5;

    // SDF + normal in one pass (shared corner-clamp computation).
    vec3 sdfResult = roundedRectSDF(centered, halfSize, ubuf.radius);
    float sdf = sdfResult.x;
    vec2 normal = sdfResult.yz;

    // Screen-space AA width from SDF derivatives. Floor 0.5px keeps thin
    // coverage without the heavier 0.75 blur on sharp 1× edges.
    float shapeAntialiasWidth = max(fwidth(sdf), 0.5);
    float fillMask = 1.0 - smoothstep(0.0, shapeAntialiasWidth, sdf);

    // ── 0 samples: fully outside the AA band ──────────────────────────
    if (fillMask <= 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    float outAlpha = fillMask * ubuf.qt_Opacity;
    float inwardDistance = max(-sdf, 0.0);

    float rimWidthPx = max(ubuf.strokeWidth, 0.0001);
    // Rim outer/inner feathers track shape AA so highlights stay aligned
    // with the coverage edge under scale / HiDPI.
    float rimBandMask =
        (1.0 - smoothstep(0.0, shapeAntialiasWidth, sdf))
        * (1.0 - smoothstep(rimWidthPx, rimWidthPx + shapeAntialiasWidth, inwardDistance));

    float configuredBezel = max(ubuf.bezelWidth, 1.0);
    float bw = max(
        min(configuredBezel, max(min(halfSize.x, halfSize.y) - shapeAntialiasWidth, 1.0)),
        1.0);

    // ── 1 sample: flat interior past bezel and rim ────────────────────
    // Slope and rim contribution are both zero; refracted == background.
    if (inwardDistance > bw && rimBandMask <= 0.0) {
        vec3 color = sampleBackdrop(texCoord).rgb;
        fragColor = vec4(clamp(color, 0.0, 1.0) * outAlpha, outAlpha);
        return;
    }

    // ── Bezel / rim band ──────────────────────────────────────────────
    // Prefer the QML unit lightDirection; renorm defensively if unbound.
    vec2 rawLightDir = ubuf.lightDirection;
    float lightLen2 = dot(rawLightDir, rawLightDir);
    vec2 lightDir = lightLen2 > 1e-8
        ? rawLightDir * inversesqrt(lightLen2)
        : vec2(-0.70710678, -0.70710678);
    // Specular is pure ALU and only non-zero in the lit rim.  The uniform
    // gate skips that work for the entire draw when highlights are disabled.
    float combinedSpecular = 0.0;
    if (ubuf.strokeStrength > 1e-4) {
        float inwardProgress = clamp(inwardDistance / max(rimWidthPx, 1.0), 0.0, 1.0);
        float strengthFalloff = 1.0 - inwardProgress * inwardProgress * 0.5;
        float normalAlignment = abs(dot(normal, lightDir));
        combinedSpecular = clamp(
            specularPower(normalAlignment, ubuf.lightPower)
                * ubuf.strokeStrength * strengthFalloff * rimBandMask,
            0.0, 1.0);
    }

    // Height profile + radiating refraction field (radius + bw).
    float bezelProgress = inwardDistance / bw;
    vec2 profileResult = convexSquircle(bezelProgress);
    // convexSquircle clamps progress; its height is exactly 1 past the bezel.
    float surfaceHeight = ubuf.thickness + profileResult.x * bw;

    // Expanded-radius normal only — distance is unused here.
    vec2 refractionNormal = roundedRectNormal(centered, halfSize, ubuf.radius + bw);
    float bevelFade = 1.0 - smoothstep(bw * 0.85, bw, inwardDistance);
    // Outer ease-in: squircle derivative peaks at the silhouette (→∞ then clamp
    // to ~85°). Without this, UV warp slams to max just inside the coverage
    // edge and high-contrast backdrop edges grow triangular spikes / hooks.
    float outerSoft = smoothstep(0.0, max(2.5 * shapeAntialiasWidth, 2.5), inwardDistance);
    float clampedSlope = min(profileResult.y * bevelFade * outerSoft, 8.0); // tan≈83°
    vec3 surfaceNormal = normalize(vec3(refractionNormal * clampedSlope, 1.0));

    float lightFacing = clamp(dot(normal, lightDir) * 0.5 + 0.5, 0.0, 1.0);
    float directionalOpticalStrength = mix(0.85, 1.15, lightFacing);

    float baseIor = max(ubuf.ior, 1.0001);
    float nz = 1.0 - surfaceNormal.z;
    float nz2 = nz * nz;
    float prismEffect = 1.0 + nz2 * 0.3;
    // Uniform gate: whole draw shares one arm (no per-pixel dispersion flicker).
    bool useDispersion = ubuf.dispersion > 1e-4;
    float disp = useDispersion
        ? (ubuf.dispersion * directionalOpticalStrength * prismEffect)
        : 0.0;

    // fillMask is already the 0..1 AA coverage; do not re-smoothstep it
    // against fwidth (which can be > 1 under scale and would crush interior
    // refraction). Soft edge fade is exactly the coverage itself.
    float displaceScale = fillMask;
    float commonScale = surfaceHeight * ubuf.displacementFactor
        * directionalOpticalStrength * displaceScale;

    // Refraction samples.
    // Only the shape AA band needs geometric 2×2 SS (coverage + normal rotate
    // inside a pixel). The bezel interior keeps a single sharp warp sample so
    // high-contrast icon edges become clean ellipses, not pale side-lobes.
    bool needEdgeSS = fillMask < 0.999;

    vec3 refractedColor;
    if (clampedSlope < 1e-4 || commonScale < 1e-4) {
        refractedColor = sampleBackdrop(texCoord).rgb;
    } else if (needEdgeSS) {
        vec2 o = vec2(0.25);
        vec3 acc =
            sampleRefractedAt(centered + vec2(-o.x, -o.y), halfSize, size, pixelSize,
                              bw, shapeAntialiasWidth, baseIor, lightDir)
          + sampleRefractedAt(centered + vec2( o.x, -o.y), halfSize, size, pixelSize,
                              bw, shapeAntialiasWidth, baseIor, lightDir)
          + sampleRefractedAt(centered + vec2(-o.x,  o.y), halfSize, size, pixelSize,
                              bw, shapeAntialiasWidth, baseIor, lightDir)
          + sampleRefractedAt(centered + vec2( o.x,  o.y), halfSize, size, pixelSize,
                              bw, shapeAntialiasWidth, baseIor, lightDir);
        refractedColor = acc * 0.25;
    } else if (useDispersion) {
        vec2 scaledPixel = commonScale * pixelSize;
        vec3 rayR = refract(kViewDir, surfaceNormal, 1.0 / max(baseIor - disp, 1.0001));
        vec3 rayG = refract(kViewDir, surfaceNormal, 1.0 / baseIor);
        vec3 rayB = refract(kViewDir, surfaceNormal, 1.0 / max(baseIor + disp, 1.0001));
        refractedColor = vec3(
            sampleBackdrop(texCoord + refractDisplacement(rayR, 1.0) * scaledPixel).r,
            sampleBackdrop(texCoord + refractDisplacement(rayG, 1.0) * scaledPixel).g,
            sampleBackdrop(texCoord + refractDisplacement(rayB, 1.0) * scaledPixel).b
        );
    } else {
        vec3 ray = refract(kViewDir, surfaceNormal, 1.0 / baseIor);
        refractedColor = sampleBackdrop(
            texCoord + refractDisplacement(ray, commonScale) * pixelSize).rgb;
    }

    float edgeInfluence = 1.0 - smoothstep(0.0, bw, inwardDistance);
    vec3 glassInterior = refractedColor;
    // Skip the luma mix when edge saturation is off or we are past the bezel.
    if (ubuf.edgeSaturation != 0.0 && edgeInfluence > 0.0)
        glassInterior = applySaturation(
            refractedColor, 1.0 + edgeInfluence * ubuf.edgeSaturation);

    // Base glass: fillMask is 1 over almost the whole interior of the shape,
    // so avoid a redundant background fetch on the common path.
    vec3 color;
    if (fillMask >= 0.999)
        color = glassInterior;
    else
        color = mix(sampleBackdrop(texCoord).rgb, glassInterior, fillMask);

    // Reflection + rim specular only where combinedSpecular contributes.
    // (highlightEnabled=false zeroes strokeStrength → this whole block is skipped.)
    if (combinedSpecular > 1e-4) {
        vec3 reflectedColor = sampleDispersed(
            texCoord, normal, ubuf.reflectionOffset, pixelSize, useDispersion);

        float fresnel = nz2 * nz; // (1 - n.z)^3
        float reflectionBlend = clamp(
            smoothstep(0.2, 0.85, dot(reflectedColor, kLumaWeights))
                * (1.0 - smoothstep(0.35, 0.85, dot(refractedColor, kLumaWeights)))
                + fresnel * 0.3,
            0.0, 1.0);

        vec3 edgeSpecularColor = mix(refractedColor, reflectedColor, reflectionBlend);
        vec3 rimReflectionTint = mix(vec3(1.0), reflectedColor, ubuf.rimReflectionStrength);
        
        vec3 specularAdd = rimReflectionTint * ubuf.specularOpacity 
                         + ubuf.highlightColor.rgb * (ubuf.highlightColor.a * 0.5);

        float specularMask = combinedSpecular * fillMask;
        color = mix(color, edgeSpecularColor, specularMask);
        color += specularAdd * specularMask;
    }

    fragColor = vec4(clamp(color, 0.0, 1.0) * outAlpha, outAlpha);
}
