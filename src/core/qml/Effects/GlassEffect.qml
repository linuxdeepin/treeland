// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects

Item {
    id: effect

    required property variant source

    // Liquid Glass material controls.
    // Blur and shadow stay delegated to Qt Quick MultiEffect / callers.
    property real radius: 12
    property real thickness: 40
    property real bezelWidth: 36
    property real ior: 1.33
    property real specular: 0.0
    property real tint: 0.0
    // Limit the geometric surface slope near the silhouette.
    property real refractionMaxTan: 2.0
    property real contentEdgePull: 0.0
    property real contentRampEnd: 0.0
    property real profilePower: 2.0
    property real innerShadow: 0.0

    // Compatibility inputs kept for existing Blur.qml users. MultiEffect owns
    // backdrop blur and colour adjustment before the refraction shader.
    property bool blurEnabled: true
    property int blurMax: 64
    property real blurAmount: 0.6
    property real blurMultiplier: 0.0
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: 0.0
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
        // Property order MUST match the UBO layout in liquidglass.frag (after qt_Matrix, qt_Opacity)
        readonly property vector2d itemSize: Qt.vector2d(Math.max(width, 1), Math.max(height, 1))
        readonly property vector2d lightDirection: effect.lightDirection
        readonly property real radius: effect.radius
        readonly property real bezelWidth: effect.bezelWidth
        readonly property real thickness: effect.thickness
        readonly property real ior: effect.ior
        readonly property real specular: Math.max(0, Math.min(1, effect.specular))
        readonly property real tint: Math.max(0, Math.min(1, effect.tint))
        readonly property real contentEdgePull: Math.max(0, Math.min(1, effect.contentEdgePull))
        readonly property real contentRampEnd: Math.max(0.001, Math.min(1, effect.contentRampEnd))
        readonly property real refractionMaxTan: Math.max(0.1, effect.refractionMaxTan)
        readonly property real profilePower: Math.max(1, effect.profilePower)
        readonly property real innerShadow: Math.max(0, Math.min(1, effect.innerShadow))

        vertexShader: "qrc:/shaders/liquidglass.vert.qsb"
        fragmentShader: "qrc:/shaders/liquidglass.frag.qsb"
    }
}
