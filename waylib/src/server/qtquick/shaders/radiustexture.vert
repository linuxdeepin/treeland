// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec2 localPosition;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec2 vLocalPosition;

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    vec4 radii;
    vec4 params; // width, height, opacity, antialiasing enabled
    float hasAlphaChannel;
} ubuf;

void main()
{
    vTexCoord = texCoord;
    vLocalPosition = localPosition;
    gl_Position = ubuf.matrix * vec4(position, 0.0, 1.0);
}
