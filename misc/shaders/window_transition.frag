// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Single-pass cross-fade for the icon-to-window transition. Both layers are
// blended in one fragment so their combined coverage stays 1 wherever the
// window is painted: cross-fading two independently translucent items would
// let the desktop through as progress * (1 - progress).
//
// Both textures cover the animation item exactly, so plain UV sampling is
// enough. The shadow is drawn by the animation item, not captured.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;   // 0: icon only, 1: window content only
} ubuf;

layout(binding = 1) uniform sampler2D windowTexture;
layout(binding = 2) uniform sampler2D iconTexture;

void main()
{
    vec4 icon = texture(iconTexture, qt_TexCoord0);
    vec4 window = texture(windowTexture, qt_TexCoord0);
    fragColor = mix(icon, window, ubuf.progress) * ubuf.qt_Opacity;
}
