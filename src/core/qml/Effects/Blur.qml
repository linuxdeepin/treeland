// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Waylib.Server
import Treeland

RenderBufferBlitter {
    id: blitter
    smooth: true

    property real radius: 0
    property bool radiusEnabled: radius > 0
    property int blurMax: Helper.config.blurStrength
    property bool blurEnabled: blurMax > 0 && blurAmount > 0
    property real blurAmount: Helper.config.blurAmount
    property real multiplier: Helper.config.blurMultiplier
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: Helper.config.glassSaturation
    property real glassSpecular: Helper.config.glassSpecular
    property real glassTint: 0.0
    property bool glassEnabled: Helper.config.glassEnabled
    property Item effectContent: contentLoader
    property bool contentOffscreen: false
    property bool effectEnabled: true

    z: parent.z ? parent.z - 1 : -1
    anchors.fill: parent

    // Dispatch between Liquid Glass and traditional blur via a Loader so only
    // the active branch is instantiated.  Toggling the DConfig key unloads one
    // Component and loads the other.
    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: blitter.glassEnabled ? glassComponent : blurComponent
        visible: blitter.effectEnabled && !blitter.contentOffscreen
    }

    Component {
        id: glassComponent
        GlassEffect {
            anchors.fill: parent
            source: blitter.content
            radius: blitter.radius
            blurEnabled: blitter.blurEnabled
            blurMax: blitter.blurMax
            blurAmount: blitter.blurAmount
            blurMultiplier: blitter.multiplier
            bezelWidth: Helper.config.glassBezel
            thickness: Helper.config.glassThickness
            ior: 1.33
            specular: blitter.glassSpecular
            tint: blitter.glassTint
            brightness: blitter.brightness
            contrast: blitter.contrast
            saturation: blitter.saturation
            contentEdgePull: 0.0
            contentRampEnd: 0.0
            refractionMaxTan: 3.3
            profilePower: Helper.config.glassProfilePower
            innerShadow: 0.0
        }
    }

    Component {
        id: blurComponent
        Item {
            anchors.fill: parent

            MultiEffect {
                id: blur
                anchors.fill: parent
                layer.enabled: blitter.radiusEnabled
                smooth: blitter.radiusEnabled
                opacity: blitter.radiusEnabled ? 0 : blitter.opacity
                source: blitter.content
                autoPaddingEnabled: false
                blurEnabled: blitter.blurEnabled
                blur: blitter.blurAmount
                blurMax: blitter.blurMax
                blurMultiplier: blitter.multiplier
                saturation: 0.2
            }

            Loader {
                x: blur.x
                y: blur.y
                active: blitter.radiusEnabled
                sourceComponent: Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeWidth: 0
                        fillItem: blur
                        PathRectangle {
                            width: blur.width
                            height: blur.height
                            radius: blitter.radius
                        }
                    }
                }
            }
        }
    }
}
