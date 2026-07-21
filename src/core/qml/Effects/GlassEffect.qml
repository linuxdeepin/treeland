// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects

Item {
    id: effect

    required property variant source

    // Liquid Glass material controls.
    // Blur and shadow stay delegated to Qt Quick MultiEffect / callers.
    property real radius: 60
    property real thickness: 50
    property real bezelWidth: 60
    property real ior: 1.5
    property real specular: 0.0
    property real tint: 0.0
    // Content UV pull vs optical path (test_glass tunable).
    property real contentEdgePull: 0.42
    property real contentRampEnd: 0.50
    property real refractionMaxTan: 2.75

    // Compatibility inputs kept for existing Blur.qml users. MultiEffect owns
    // backdrop blur and colour adjustment before the refraction shader.
    property bool blurEnabled: true
    property int blurMax: 12
    property real blurAmount: 0.6
    property real blurMultiplier: 0.0
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: 0.04
    readonly property vector2d lightDirection: Qt.vector2d(0.5, -0.7)
    readonly property bool multiEffectEnabled:
        (blurEnabled && blurAmount > 0 && blurMax > 0)
        || brightness != 0 || contrast != 0 || saturation != 0


    MultiEffect {
        id: processedSource
        anchors.fill: parent
        visible: effect.multiEffectEnabled
        source: effect.source
        autoPaddingEnabled: false
        blurEnabled: effect.blurEnabled
        blur: effect.blurAmount
        blurMax: effect.blurMax
        blurMultiplier: effect.blurMultiplier
        brightness: effect.brightness
        contrast: effect.contrast
        saturation: effect.saturation
    }
    ShaderEffectSource {
        id: processedTexture
        anchors.fill: parent
        sourceItem: processedSource
        hideSource: true
        live: true
        visible: false
    }

    ShaderEffect {
        id: glassShader
        objectName: "glassShader"
        anchors.fill: parent
        smooth: true
        property variant source: effect.multiEffectEnabled ? processedTexture : effect.source
        readonly property vector2d itemSize: Qt.vector2d(Math.max(width, 1), Math.max(height, 1))
        readonly property real radius: effect.radius
        readonly property real bezelWidth: effect.bezelWidth
        readonly property real tint: Math.max(0, Math.min(1, effect.tint))
        readonly property real contentEdgePull: Math.max(0, Math.min(1, effect.contentEdgePull))
        readonly property real contentRampEnd: Math.max(0.05, Math.min(1, effect.contentRampEnd))
        readonly property real refractionMaxTan: Math.max(0.1, effect.refractionMaxTan)
        readonly property real thickness: effect.thickness
        readonly property real ior: effect.ior
        readonly property real specular: Math.max(0, Math.min(1, effect.specular))

        readonly property vector2d lightDirection: effect.lightDirection

        vertexShader: "qrc:/shaders/liquidglass.vert.qsb"
        fragmentShader: "qrc:/shaders/liquidglass.frag.qsb"
    }
}
