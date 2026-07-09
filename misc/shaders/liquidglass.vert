// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 texCoord;

layout(std140, binding = 0) uniform vert_buf {
    mat4 qt_Matrix;
    float qt_Opacity;
} vbuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    texCoord = qt_MultiTexCoord0;
    gl_Position = vbuf.qt_Matrix * qt_Vertex;
}
