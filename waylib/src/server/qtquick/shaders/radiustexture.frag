// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec2 vLocalPosition;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    vec4 radii; // top-left, top-right, bottom-right, bottom-left
    vec4 params; // width, height, opacity, antialiasing enabled
    float hasAlphaChannel;
} ubuf;

layout(binding = 1) uniform sampler2D sourceTexture;

float cornerRadius(vec2 point)
{
    vec2 side = step(vec2(0.0), point);
    float topRadius = mix(ubuf.radii.x, ubuf.radii.y, side.x);
    float bottomRadius = mix(ubuf.radii.w, ubuf.radii.z, side.x);
    return mix(topRadius, bottomRadius, side.y);
}

float roundedBoxDistance(vec2 point, vec2 halfSize, float radius)
{
    vec2 q = abs(point) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void main()
{
    vec2 halfSize = ubuf.params.xy * 0.5;
    vec2 point = vLocalPosition - halfSize;
    float distance = roundedBoxDistance(point, halfSize, cornerRadius(point));
    float antialiasWidth = max(fwidth(distance) * 0.5, 0.0001);
    float smoothAlpha = 1.0 - smoothstep(-antialiasWidth, antialiasWidth, distance);
    float hardAlpha = 1.0 - step(0.0, distance);
    float alpha = mix(hardAlpha, smoothAlpha, step(0.5, ubuf.params.w));
    vec4 color = texture(sourceTexture, vTexCoord);
    color.a = mix(1.0, color.a, ubuf.hasAlphaChannel);
    fragColor = color * (ubuf.params.z * alpha);
}
