// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects

Item {
    id: effect

    required property variant source

    property real radius: 0
    property bool blurEnabled: false
    property int blurMax: 32
    property real blurAmount: 1.0
    property real blurMultiplier: 0.0

    // Glass material parameters (aligned with liquid-dom naming)
    property real bezelWidth: 30        // edge bevel width (px)
    property real thickness: 50         // base glass thickness (px)
    property real displacementFactor: 1 // scalar on displacement
    property real ior: 1.2              // refractive index
    property real dispersion: 0.02     // RGB channel separation

    // Colour controls — applied to the backdrop BEFORE refraction, so the
    // glass specular / rim highlights stay sharp and uncoloured.
    // Delegated to MultiEffect (avoid duplicate implementation)
    property real brightness: 0.0       // [-1, 1], 0 = no change
    property real contrast: 0.0         // [-1, 1], 0 = no change
    property real saturation: 0.4       // [-1, 1], 0 = no change
    property real colorization: 0.0     // [0, 1], 0 = no tint
    property color colorizationColor: Qt.rgba(1, 1, 1, 1)

    // Edge-local saturation boost — MultiEffect can't do spatially-varying, so
    // this stays in the shader.  Only affects the bezel zone.
    property real edgeSaturation: 0.0

    // Specular / highlight controls
    property color highlightColor: Qt.rgba(1, 1, 1, 0.35)
    property real strokeWidth: 1.5      // specular rim band width (px)
    property real strokeStrength: 1.0   // specular strength
    property real specularOpacity: 0.6  // white specular opacity (liquid-dom demo value)
    property bool highlightEnabled: false  // master toggle for edge specular highlights

    // Rim reflection tint: when enabled, the white specular rim picks up a
    // small amount of nearby backdrop colour.  The shader receives only a
    // float gate to avoid bool/int uniform binding issues on GLES drivers.
    property bool rimReflectionEnabled: true

    // Light direction.  Degrees; 0 points right, -90 points up.
    property real lightAngle: -135.0
    readonly property real lightAngleRadians: lightAngle * Math.PI / 180
    property real lightPower: 2.0        // specular sharpness exponent
    readonly property vector2d lightDirection: Qt.vector2d(
        Math.cos(effect.lightAngleRadians),
        Math.sin(effect.lightAngleRadians))

    // Reflection
    property real reflectionOffset: 18   // reflection sampling offset (px)


    // True when MultiEffect is needed for blur or colour grading of the backdrop
    readonly property bool multiEffectEnabled:
        (blurEnabled && blurAmount > 0 && blurMax > 0)
        || brightness != 0 || contrast != 0 || saturation != 0 || colorization > 0

    anchors.fill: parent

    // ── Stage 1: blur + colour-grade the raw backdrop ──────────────────
    // NOT directly visible — its layer texture is sampled by the glass
    // shader.  Blur happens here so the glass specular / rim highlights
    // added in Stage 2 stay sharp.
    MultiEffect {
        id: blurredSource
        anchors.fill: parent
        visible: effect.multiEffectEnabled
        layer.enabled: effect.multiEffectEnabled
        smooth: true
        opacity: 0   // not drawn to screen; sampled as a texture
        source: effect.source
        autoPaddingEnabled: false
        blurEnabled: effect.blurEnabled && effect.blurAmount > 0
        blur: blurEnabled ? effect.blurAmount : 0.0
        blurMax: effect.blurMax
        blurMultiplier: effect.blurMultiplier
        brightness: effect.brightness
        contrast: effect.contrast
        saturation: effect.saturation
        colorization: effect.colorization
        colorizationColor: effect.colorizationColor
    }

    // ── Stage 2: glass shader refracts the (blurred) backdrop and adds
    // specular highlights / rim light on top.  This item intentionally
    // owns no rounded Shape clip; wrappers such as Glass.qml provide that.
    ShaderEffect {
        id: glassShader
        objectName: "glassShader"
        anchors.fill: parent
        smooth: true // Enables linear filtering for the sampler to prevent jagged steps
        property variant source: effect.multiEffectEnabled ? blurredSource : effect.source
        readonly property vector2d itemSize: Qt.vector2d(Math.max(width, 1), Math.max(height, 1))
        readonly property real radius: effect.radius
        readonly property real bezelWidth: effect.bezelWidth
        readonly property real thickness: effect.thickness
        readonly property real displacementFactor: effect.displacementFactor
        readonly property real ior: effect.ior
        readonly property real dispersion: effect.dispersion
        readonly property color highlightColor: effect.highlightColor
        readonly property real strokeWidth: effect.strokeWidth
        readonly property real strokeStrength: effect.highlightEnabled ? effect.strokeStrength : 0.0
        readonly property vector2d lightDirection: effect.lightDirection
        readonly property real lightPower: effect.lightPower
        readonly property real edgeSaturation: effect.edgeSaturation
        readonly property real reflectionOffset: effect.reflectionOffset
        readonly property real specularOpacity: effect.highlightEnabled ? effect.specularOpacity : 0.0
        readonly property real rimReflectionStrength: effect.rimReflectionEnabled ? 0.22 : 0.0
        vertexShader: "qrc:/shaders/liquidglass.vert.qsb"
        fragmentShader: "qrc:/shaders/liquidglass.frag.qsb"
    }
}
