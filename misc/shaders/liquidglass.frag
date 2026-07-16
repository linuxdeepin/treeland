// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

// Liquid Glass fragment shader for treeland.
//
// Algorithm pipeline:
//   1. SDF of rounded rectangle
//   2. Convex-squircle surface height profile
//   3. Snell refraction displacement
//   4. Specular edge highlight (fixed light direction)
//   5. Inner shadow
//   6. Inner rim glow
//   7. White tint mix
//   8. Alpha anti-aliasing
//
// Blur is handled externally by MultiEffect; this shader samples the
// (already blurred) source texture with a single bilinear tap.
// Outer shadow is handled by MultiEffect in the QML layer.

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    float radius;           // corner radius (px)
    float bezelWidth;       // edge bevel width (px)
    float thickness;        // base glass thickness (px)
    float ior;              // refractive index
    float specularOpacity;  // specular highlight strength
    float tintOpacity;      // white tint mix amount
} ubuf;

layout(binding = 1) uniform sampler2D source;

// Signed distance to a rounded rectangle.
float sdRoundedRect(vec2 p, vec2 halfSize, float r)
{
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

// Convex-squircle height profile for the beveled edge.
float surfaceHeight(float t)
{
    float s = 1.0 - t;
    return pow(1.0 - s * s * s * s, 0.25);
}

void main()
{
    // Convert UV to pixel space relative to glass center.
    vec2 screenPx = texCoord * ubuf.itemSize;
    vec2 p = screenPx - ubuf.itemSize * 0.5;
    vec2 halfSize = ubuf.itemSize * 0.5;

    float sd = sdRoundedRect(p, halfSize, ubuf.radius);

    // Outside the glass: fully transparent (outer shadow is handled by
    // MultiEffect in the QML layer).
    if (sd > 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    // Distance from the edge, clamped to the bezel zone.
    float distFromEdge = -sd;
    float bezel = min(ubuf.bezelWidth,
                      min(ubuf.radius, min(halfSize.x, halfSize.y)) - 1.0);
    float t = clamp(distFromEdge / bezel, 0.0, 1.0);

    // Surface height and its derivative.
    float h = surfaceHeight(t);
    float dt = 0.001;
    float h2 = surfaceHeight(min(t + dt, 1.0));
    float dh = (h2 - h) / dt;

    // Snell refraction displacement.
    float slopeAngle = atan(dh * (ubuf.thickness / bezel));
    float sinR = sin(slopeAngle) / ubuf.ior;
    sinR = clamp(sinR, -1.0, 1.0);
    float thetaR = asin(sinR);
    float displacement = h * ubuf.thickness * (tan(slopeAngle) - tan(thetaR));

    // Gradient of the SDF for displacement direction.
    vec2 grad;
    float eps = 0.5;
    grad.x = sdRoundedRect(p + vec2(eps, 0.0), halfSize, ubuf.radius) - sd;
    grad.y = sdRoundedRect(p + vec2(0.0, eps), halfSize, ubuf.radius) - sd;
    grad = normalize(grad);

    // Sample the (pre-blurred) backdrop at the refracted position.
    vec2 offset = -grad * displacement / ubuf.itemSize;
    vec2 refractedUV = texCoord + offset;
    vec3 color = texture(source, refractedUV).rgb;

    // Specular edge highlight (fixed light direction).
    vec2 lightDir = normalize(vec2(0.5, -0.7));
    float rimDot = abs(dot(grad, lightDir));
    float rimFalloff = 1.0 - smoothstep(0.0, bezel * 0.4, distFromEdge);
    float specHighlight = pow(rimDot * rimFalloff, 1.5);
    color += vec3(specHighlight * ubuf.specularOpacity);

    // Inner shadow.
    float innerShadow = 1.0 - smoothstep(0.0, bezel * 0.6, distFromEdge);
    color *= mix(1.0, 0.7, innerShadow * 0.3);

    // Inner rim glow.
    float innerRim = smoothstep(0.0, 2.0, distFromEdge)
                   * (1.0 - smoothstep(2.0, 5.0, distFromEdge));
    color += vec3(innerRim * 0.15 * ubuf.specularOpacity);

    // White tint.
    color = mix(color, vec3(1.0), ubuf.tintOpacity);

    // Alpha anti-aliasing.
    float alpha = smoothstep(0.0, 1.5, distFromEdge);
    fragColor = vec4(color * alpha, alpha) * ubuf.qt_Opacity;
}
