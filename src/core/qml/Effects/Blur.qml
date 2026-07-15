// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Waylib.Server
import Treeland

Item {
    id: root
    smooth: true

    property real radius: 0
    property bool radiusEnabled: radius > 0
    property int blurMax: Helper.config.blurStrength
    property bool blurEnabled: blurMax > 0 && blurAmount > 0
    property real blurAmount: Helper.config.blurAmount
    property real multiplier: Helper.config.blurMultiplier
    property real brightness: Helper.config.glassBrightness
    property real lightAngle: Helper.config.glassLightAngle
    property bool highlightEnabled: Helper.config.glassHighlightEnabled
    property bool glassEnabled: Helper.config.glassEnabled

    z: parent.z ? parent.z - 1 : -1
    anchors.fill: parent

    // On Vulkan the RenderBufferBlitter backdrop-capture path is broken, so
    // skip creating it (and all dependent effect components) entirely and
    // fall back to a plain semi-transparent rectangle.
    Loader {
        anchors.fill: parent
        sourceComponent: WaylibHelper.isVulkanBackend ? vulkanFallback : blitterContent
    }

    Component {
        id: blitterContent
        RenderBufferBlitter {
            id: blitter
            smooth: true
            anchors.fill: parent

            // Dispatch between Liquid Glass and traditional blur via a Loader so only
            // the active branch is instantiated.  Toggling the DConfig key unloads one
            // Component and loads the other.
            Loader {
                anchors.fill: parent
                sourceComponent: blitter.glassEnabled ? glassComponent : blurComponent
            }

            Component {
                id: glassComponent
                GlassEffect {
                    anchors.fill: parent
                    source: blitter.content
                    radius: root.radius
                    blurEnabled: root.blurEnabled
                    blurMax: root.blurMax
                    blurAmount: root.blurAmount
                    blurMultiplier: root.multiplier
                    brightness: root.brightness
                    highlightEnabled: root.highlightEnabled
                    lightAngle: root.lightAngle

                    bezelWidth: Helper.config.glassBezel
                    thickness: Helper.config.glassThickness
                    displacementFactor: Helper.config.glassDisplacementFactor
                    ior: Helper.config.glassIor
                    dispersion: Helper.config.glassDispersion
                    contrast: -0.12
                    saturation: 0.4
                    colorization: 0.12
                    edgeSaturation: Helper.config.glassEdgeSaturation
                    highlightColor: Qt.rgba(1, 1, 1, 0.3)
                    strokeWidth: 0.5
                    strokeStrength: 1.5
                    specularOpacity: 0.82
                    rimReflectionEnabled: true
                    lightPower: 3.0
                    reflectionOffset: Helper.config.glassReflectionOffset
                }
            }

            Component {
                id: blurComponent
                Item {
                    anchors.fill: parent

                    MultiEffect {
                        id: blur
                        anchors.fill: parent
                        layer.enabled: root.radiusEnabled
                        smooth: root.radiusEnabled
                        opacity: root.radiusEnabled ? 0 : root.opacity
                        source: blitter.content
                        autoPaddingEnabled: false
                        blurEnabled: root.blurEnabled
                        blur: root.blurAmount
                        blurMax: root.blurMax
                        blurMultiplier: root.multiplier
                        saturation: 0.2
                    }

                    Loader {
                        x: blur.x
                        y: blur.y
                        active: root.radiusEnabled
                        sourceComponent: Shape {
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeWidth: 0
                                fillItem: blur
                                PathRectangle {
                                    width: blur.width
                                    height: blur.height
                                    radius: root.radius
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: vulkanFallback
        Rectangle {
            radius: root.radius
            color: Qt.rgba(0, 0, 0, 0.3)
        }
    }
}
