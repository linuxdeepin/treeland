// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    vec2 lightDirection;
    float radius;
    float bezelWidth;
    float thickness;
    float ior;
    float specular;
    float tint;
    float contentEdgePull;    // fraction of optical pull kept at the glass lip (0–1)
    float contentRampEnd;     // t in bezel where content pull reaches full (0–1)
    float refractionMaxTan;   // cap on geometric slope tan(theta)
} ubuf;

layout(binding = 1) uniform sampler2D source;

float sdRoundedRect(vec2 p, vec2 halfSize, float r)
{
    float rr = min(max(r, 0.0), min(halfSize.x, halfSize.y));
    vec2 q = abs(p) - halfSize + rr;
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - rr;
}

vec2 roundedRectNormal(vec2 p, vec2 halfSize, float r)
{
    float rr = min(max(r, 0.0), min(halfSize.x, halfSize.y));
    vec2 edgeDistance = halfSize - abs(p);
    vec2 side = mix(vec2(-1.0), vec2(1.0), step(vec2(0.0), p));

    vec2 straightNormal = edgeDistance.x < edgeDistance.y
        ? vec2(side.x, 0.0)
        : vec2(0.0, side.y);

    vec2 cornerCenter = side * (halfSize - vec2(rr));
    vec2 cornerVector = p - cornerCenter;
    float cornerLength = length(cornerVector);
    vec2 cornerNormal = cornerLength > 0.0001
        ? cornerVector / cornerLength
        : normalize(straightNormal + side);

    float cornerBlend = max(min(rr, min(halfSize.x, halfSize.y)) * 0.08, 1.0);
    float cornerMask = step(0.5, rr)
        * (1.0 - smoothstep(rr - cornerBlend, rr + cornerBlend, edgeDistance.x))
        * (1.0 - smoothstep(rr - cornerBlend, rr + cornerBlend, edgeDistance.y));

    return normalize(mix(straightNormal, cornerNormal, cornerMask));
}

float surfaceHeight(float t)
{
    // Squircle-like profile: mostly flat interior, rise concentrated near the rim.
    float s = 1.0 - t;
    return pow(1.0 - s * s * s * s, 0.25);
}

vec3 sampleBg(vec2 uv)
{
    return texture(source, clamp(uv, vec2(0.002), vec2(0.998))).rgb;
}

// Average only in-bounds taps. Offsets are tangent + inward only.
vec3 sampleBgSoft(vec2 uv, vec2 resolution, float radiusPx, vec2 inward)
{
    if (radiusPx < 0.5)
        return sampleBg(uv);

    vec2 invRes = vec2(1.0) / max(resolution, vec2(1.0));
    vec2 inN = length(inward) > 1e-5 ? normalize(inward) : vec2(0.0);
    vec2 tang = vec2(-inN.y, inN.x);
    float r = radiusPx;

    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    vec2 offs[6];
    float ws[6];
    offs[0] = vec2(0.0);              ws[0] = 0.40;
    offs[1] = tang * r;               ws[1] = 0.15;
    offs[2] = -tang * r;              ws[2] = 0.15;
    offs[3] = inN * r;                ws[3] = 0.15;
    offs[4] = (inN + tang) * r * 0.7; ws[4] = 0.075;
    offs[5] = (inN - tang) * r * 0.7; ws[5] = 0.075;

    for (int i = 0; i < 6; ++i) {
        vec2 s = uv + offs[i] * invRes;
        if (s.x <= 0.0 || s.x >= 1.0 || s.y <= 0.0 || s.y >= 1.0)
            continue;
        acc += texture(source, s).rgb * ws[i];
        wsum += ws[i];
    }
    if (wsum < 1e-4)
        return sampleBg(uv);
    return acc / wsum;
}

void main()
{
    vec2 size = max(ubuf.itemSize, vec2(1.0));
    vec2 p = texCoord * size - size * 0.5;
    vec2 halfSize = size * 0.5;

    float sd = sdRoundedRect(p, halfSize, ubuf.radius);

    // Anti-aliased shape edge: full band ≈ 4 logical pixels (2*edgeAA).
    float scale = length(vec2(dFdx(p.x), dFdy(p.x)));
    float edgeAA = max(scale * 2.0, 2.0);
    float shapeAlpha = smoothstep(edgeAA, -edgeAA, sd);

    if (shapeAlpha <= 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    float aa = max(scale * 2.5, 1.75);

    float distFromEdge = -sd;
    float straightMaxBezel = max(min(halfSize.x, halfSize.y) - 1.0, 1.0);
    float cornerMaxBezel = max(min(ubuf.radius, min(halfSize.x, halfSize.y)) - 1.0, 1.0);
    vec2 edgeDistance = halfSize - abs(p);
    float cornerFeather = max(abs(ubuf.bezelWidth - ubuf.radius), 1.0);
    float cornerMask = step(0.5, ubuf.radius)
        * (1.0 - smoothstep(ubuf.radius, ubuf.radius + cornerFeather, edgeDistance.x))
        * (1.0 - smoothstep(ubuf.radius, ubuf.radius + cornerFeather, edgeDistance.y));
    float maxBezel = mix(straightMaxBezel, cornerMaxBezel, cornerMask);
    float bezel = max(min(max(ubuf.bezelWidth, 1.0), maxBezel), 1.0);
    float t = clamp(distFromEdge / bezel, 0.0, 1.0);

    float h = surfaceHeight(t);
    float dt = 0.001;
    float h2 = surfaceHeight(min(t + dt, 1.0));
    float dh = (h2 - h) / dt;

    float thick = max(ubuf.thickness, 0.0);
    float rawTan = abs(dh) * (thick / max(bezel, 1.0));
    float maxTan = max(ubuf.refractionMaxTan, 0.1);
    float slopeMag = min(rawTan, maxTan);
    float slopeAngle = atan(slopeMag);

    vec2 n2 = roundedRectNormal(p, halfSize, ubuf.radius);
    vec3 N = normalize(vec3(n2 * slopeMag, 1.0));
    float H = h * thick;

    float edgePull = clamp(ubuf.contentEdgePull, 0.0, 1.0);
    float rampEnd = clamp(ubuf.contentRampEnd, 0.05, 1.0);
    float contentRamp = mix(edgePull, 1.0, smoothstep(0.0, rampEnd, t));
    float maxDisp = min(min(bezel * 0.85, thick * 0.75), 48.0);

    float iorG = max(ubuf.ior, 1.0001);

    // Mono refraction: continuous inward normal field, Snell magnitude.
    float etaG = 1.0 / iorG;
    float sinI = slopeMag / max(sqrt(slopeMag * slopeMag + 1.0), 1e-4);
    float sinTG = clamp(sinI * etaG, 0.0, 0.999);
    float magG = H * contentRamp * max(tan(slopeAngle) - tan(asin(sinTG)), 0.0);
    magG = min(magG, maxDisp);
    vec2 inward = -n2;
    vec2 offG = inward * magG;

    float edgeFilterPx = 1.0 * (1.0 - smoothstep(0.0, max(aa * 3.0, 3.0), distFromEdge));
    vec2 uvG = clamp(texCoord + offG / size, vec2(0.002), vec2(0.998));
    vec3 color = sampleBgSoft(uvG, size, edgeFilterPx, inward);


    vec3 V = vec3(0.0, 0.0, 1.0);
    float cosNV = clamp(dot(N, V), 0.0, 1.0);
    float F0 = 0.04;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - cosNV, 5.0);
    vec2 lightDir = normalize(ubuf.lightDirection);
    float rimDot = abs(dot(n2, lightDir));
    float rimFalloff = 1.0 - smoothstep(0.0, bezel * 0.45, distFromEdge);
    float specHighlight = fresnel * pow(rimDot * rimFalloff, 1.35);
    color += vec3(specHighlight * clamp(ubuf.specular, 0.0, 1.0));

    float innerShadow = 1.0 - smoothstep(0.0, bezel * 0.6, distFromEdge);
    color *= mix(1.0, 0.75, innerShadow * 0.25);

    float innerRim = smoothstep(0.0, 2.0, distFromEdge)
        * (1.0 - smoothstep(2.0, 5.0, distFromEdge));
    color += vec3(innerRim * 0.12 * clamp(ubuf.specular, 0.0, 1.0));

    color = mix(color, vec3(1.0), clamp(ubuf.tint, 0.0, 1.0));

    float alpha = shapeAlpha * ubuf.qt_Opacity;
    fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha);
}
