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

    // Glass material parameters
    property real bezelWidth: 60         // edge bevel width (px)
    property real thickness: 50          // base glass thickness (px)
    property real ior: 3.0               // refractive index
    property real specularOpacity: 0.55  // specular highlight strength
    property real tintOpacity: 0.08      // white tint mix amount
    property real shadow: 0.5            // outer shadow strength (MultiEffect)

    readonly property bool multiEffectEnabled:
        blurEnabled && blurAmount > 0 && blurMax > 0
    readonly property bool shadowEnabled: shadow > 0

    anchors.fill: parent

    // ── Stage 1: blur the raw backdrop ───────────────────────────────
    // Not drawn to screen; its layer texture is sampled by Stage 2.
    MultiEffect {
        id: blurredSource
        anchors.fill: parent
        visible: effect.multiEffectEnabled
        layer.enabled: effect.multiEffectEnabled
        smooth: true
        opacity: 0
        source: effect.source
        autoPaddingEnabled: false
        blurEnabled: effect.blurEnabled && effect.blurAmount > 0
        blur: blurEnabled ? effect.blurAmount : 0.0
        blurMax: effect.blurMax
        blurMultiplier: effect.blurMultiplier
    }

    // ── Stage 2: glass shader refracts the (blurred) backdrop ───────
    // Not drawn to screen; its layer texture is sampled by Stage 3.
    ShaderEffect {
        id: glassShader
        objectName: "glassShader"
        anchors.fill: parent
        smooth: true
        opacity: 0
        layer.enabled: true
        property variant source: effect.multiEffectEnabled ? blurredSource : effect.source
        readonly property vector2d itemSize: Qt.vector2d(Math.max(width, 1), Math.max(height, 1))
        readonly property real radius: effect.radius
        readonly property real bezelWidth: effect.bezelWidth
        readonly property real thickness: effect.thickness
        readonly property real ior: effect.ior
        readonly property real specularOpacity: effect.specularOpacity
        readonly property real tintOpacity: effect.tintOpacity
        vertexShader: "qrc:/shaders/liquidglass.vert.qsb"
        fragmentShader: "qrc:/shaders/liquidglass.frag.qsb"
    }

    // ── Stage 3: final output with outer shadow ─────────────────────
    // autoPaddingEnabled expands the item beyond the source size so the
    // shadow can overflow the glass bounds.
    MultiEffect {
        id: shadowOutput
        width: effect.width
        height: effect.height
        source: glassShader
        autoPaddingEnabled: effect.shadowEnabled
        shadowEnabled: effect.shadowEnabled
        shadowOpacity: effect.shadow
        shadowColor: "black"
        shadowBlur: 0.5
        shadowScale: 1.0
    }
}
