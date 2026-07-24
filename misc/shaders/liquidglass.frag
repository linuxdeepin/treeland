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
    vec2 inner = halfSize - vec2(rr);
    vec2 delta = p - clamp(p, -inner, inner);
    float len2 = dot(delta, delta);
    if (len2 > 1e-8)
        return delta * inversesqrt(len2);

    vec2 edgeDistance = halfSize - abs(p);
    float useX = step(edgeDistance.x, edgeDistance.y);
    return vec2(sign(p.x) * useX, sign(p.y) * (1.0 - useX));
}

float smoother01(float v)
{
    v = clamp(v, 0.0, 1.0);
    return v * v * v * (v * (v * 6.0 - 15.0) + 10.0);
}

float getFieldT(vec2 p, vec2 halfSize, float outerRadius, float bezel)
{
    float rr = min(max(outerRadius, 0.0), min(halfSize.x, halfSize.y));
    vec2 d = halfSize - abs(p);

    // outerX/Y: how far the corner arc extends into the x/y direction.
    float outerX = d.x < rr ? rr - sqrt(max(0.0, d.x * (2.0 * rr - d.x))) : 0.0;
    float outerY = d.y < rr ? rr - sqrt(max(0.0, d.y * (2.0 * rr - d.y))) : 0.0;

    // Directional bezel progress, adjusted for corner arc offset.
    float topProgress = clamp((d.y - outerX) / bezel, 0.0, 1.0);
    float sideProgress = clamp((d.x - outerY) / bezel, 0.0, 1.0);

    // Geometric mean of smoothed progressions: ensures t=0 at silhouette,
    // t=1 at flat region, and a smooth blend in corners when radius < bezel.
    // Replaces the old binary top/side split which produced a fold line.
    return sqrt(smoother01(topProgress) * smoother01(sideProgress));
}

vec4 expandingCornerBezelField(vec2 p, vec2 halfSize, float outerRadius, float bezel)
{
    float t = getFieldT(p, halfSize, outerRadius, bezel);
    
    float eps = 0.5;
    float tx = getFieldT(p + vec2(eps, 0.0), halfSize, outerRadius, bezel);
    float tnx = getFieldT(p - vec2(eps, 0.0), halfSize, outerRadius, bezel);
    float ty = getFieldT(p + vec2(0.0, eps), halfSize, outerRadius, bezel);
    float tny = getFieldT(p - vec2(0.0, eps), halfSize, outerRadius, bezel);
    
    vec2 grad = vec2(tx - tnx, ty - tny) / (2.0 * eps);
    float gradLen = length(grad);
    
    float localBezel = gradLen > 1e-5 ? 1.0 / gradLen : bezel;
    vec2 normal = gradLen > 1e-5 ? -(grad / gradLen) : roundedRectNormal(p, halfSize, outerRadius);
    
    return vec4(t, localBezel, normal);
}

vec2 surfaceProfile(float t)
{
    // Squircle-like profile and analytic derivative.
    float s = 1.0 - clamp(t, 0.0, 1.0);
    float s2 = s * s;
    float s4 = s2 * s2;
    float inside = max(1.0 - s4, 1e-4);
    float height = sqrt(sqrt(inside));
    float derivative = s2 * s * height / inside;
    return vec2(height, derivative);
}

vec3 sampleBg(vec2 uv)
{
    return texture(source, clamp(uv, vec2(0.002), vec2(0.998))).rgb;
}


void main()
{
    vec2 size = max(ubuf.itemSize, vec2(1.0));
    vec2 p = texCoord * size - size * 0.5;
    vec2 halfSize = size * 0.5;

    float sd = sdRoundedRect(p, halfSize, ubuf.radius);

    // Anti-aliased shape edge: full band ≈ 4 logical pixels (2*edgeAA).
    float scale = length(vec2(dFdx(sd), dFdy(sd)));
    float edgeAA = max(scale * 2.0, 2.0);
    float shapeAlpha = smoothstep(edgeAA, -edgeAA, sd);

    if (shapeAlpha <= 0.0) {
        fragColor = vec4(0.0);
        return;
    }


    float distFromEdge = -sd;
    float configuredBezel = max(ubuf.bezelWidth, 1.0);
    float outerRadius = min(max(ubuf.radius, 0.0), min(halfSize.x, halfSize.y));
    float maxSizeBezel = max(min(halfSize.x, halfSize.y) - 1.0, 1.0);
    float sizeLimitedBezel = min(configuredBezel, maxSizeBezel);
    
    bool expandingCorners = outerRadius < sizeLimitedBezel;
    float effectExtent = expandingCorners ? sizeLimitedBezel * 1.5 : sizeLimitedBezel;
    
    if (ubuf.specular > 1e-4)
        effectExtent = max(effectExtent, 5.0);

    if (distFromEdge > effectExtent) {
        vec3 color = sampleBg(texCoord);
        if (ubuf.tint > 1e-4)
            color = mix(color, vec3(1.0), clamp(ubuf.tint, 0.0, 1.0));
        float alpha = shapeAlpha * ubuf.qt_Opacity;
        fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha);
        return;
    }

    float bezel;
    float t;
    float localBezel;
    vec2 n2;

    if (expandingCorners) {
        bezel = sizeLimitedBezel;
        vec4 field = expandingCornerBezelField(p, halfSize, outerRadius, bezel);
        t = field.x;
        localBezel = field.y;
        n2 = field.zw;
    } else {
        bezel = sizeLimitedBezel;
        t = clamp(distFromEdge / bezel, 0.0, 1.0);
        localBezel = bezel;
        n2 = roundedRectNormal(p, halfSize, outerRadius);
    }

    if (expandingCorners && t >= 1.0 && (ubuf.specular <= 1e-4 || distFromEdge >= 5.0)) {
        vec3 color = sampleBg(texCoord);
        if (ubuf.tint > 1e-4)
            color = mix(color, vec3(1.0), clamp(ubuf.tint, 0.0, 1.0));
        float alpha = shapeAlpha * ubuf.qt_Opacity;
        fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha);
        return;
    }

    vec2 profile = surfaceProfile(t);
    float h = profile.x;

    float thick = max(ubuf.thickness, 0.0);
    // Ease refraction in from the silhouette; the profile derivative peaks at
    // the edge and otherwise turns high-contrast corners into thin spikes.
    float outerSoft = smoothstep(0.0, max(2.5 * edgeAA, 2.5), distFromEdge);
    float rawTan = profile.y * (thick / localBezel);
    float maxTan = max(ubuf.refractionMaxTan, 0.1);
    float slopeMag = min(rawTan, maxTan) * outerSoft;

    float H = h * thick;

    float edgePull = clamp(ubuf.contentEdgePull, 0.0, 1.0);
    float rampEnd = clamp(ubuf.contentRampEnd, 0.05, 1.0);
    float contentRamp = mix(edgePull, 1.0, smoothstep(0.0, rampEnd, t));
    float maxDisp = min(min(bezel * 0.85, thick * 0.75), 48.0);

    float iorG = max(ubuf.ior, 1.0001);

    // Mono refraction: continuous inward normal field and algebraic Snell magnitude.
    float incidentInvLen = inversesqrt(slopeMag * slopeMag + 1.0);
    float sinI = slopeMag * incidentInvLen;
    float sinT = clamp(sinI / iorG, 0.0, 0.999);
    float tanT = sinT * inversesqrt(max(1.0 - sinT * sinT, 1e-4));
    float magG = H * contentRamp * max(slopeMag - tanT, 0.0);
    magG = min(magG, maxDisp);
    vec2 inward = -n2;
    vec2 offG = inward * magG;

    vec2 uvG = clamp(texCoord + offG / size, vec2(0.002), vec2(0.998));
    vec3 color = sampleBg(uvG);


    if (ubuf.specular > 1e-4) {
        vec3 N = normalize(vec3(n2 * slopeMag, 1.0));
        float oneMinusCos = 1.0 - clamp(N.z, 0.0, 1.0);
        float oneMinusCos2 = oneMinusCos * oneMinusCos;
        float fresnel = 0.04 + 0.96 * oneMinusCos2 * oneMinusCos2 * oneMinusCos;
        vec2 lightDir = normalize(ubuf.lightDirection);
        float rimDot = abs(dot(n2, lightDir));
        float rimFalloff = 1.0 - smoothstep(0.0, bezel * 0.45, distFromEdge);
        float specHighlight = fresnel * pow(rimDot * rimFalloff, 1.35);
        color += vec3(specHighlight * clamp(ubuf.specular, 0.0, 1.0));

        float innerRim = smoothstep(0.0, 2.0, distFromEdge)
            * (1.0 - smoothstep(2.0, 5.0, distFromEdge));
        color += vec3(innerRim * 0.12 * clamp(ubuf.specular, 0.0, 1.0));
    }

    float innerShadow = 1.0 - smoothstep(0.0, bezel * 0.6, distFromEdge);
    color *= mix(1.0, 0.75, innerShadow * 0.25);


    if (ubuf.tint > 1e-4)
        color = mix(color, vec3(1.0), clamp(ubuf.tint, 0.0, 1.0));

    float alpha = shapeAlpha * ubuf.qt_Opacity;
    fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha);
}
