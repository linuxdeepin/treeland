// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import TestGlass

Item {
    id: root
    width: 256
    height: 256

    /// Expose the GlassEffect instance for property manipulation from C++.
    property alias glass: glassEffect
    property alias backdropVisible: backdrop.visible
    property alias glassXScale: glassScale.xScale
    property alias glassYScale: glassScale.yScale
    property bool cornerProbeVisible: false

    /// A colourful, high-contrast backdrop so refraction / highlights / rim
    /// tint are clearly visible in grabbed images.
    Rectangle {
        id: backdrop
        anchors.fill: parent
        visible: true
        layer.enabled: true

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#c0392b" }
            GradientStop { position: 0.33; color: "#27ae60" }
            GradientStop { position: 0.66; color: "#2980b9" }
            GradientStop { position: 1.0; color: "#8e44ad" }
        }

        // Distinct shapes for refraction / displacement visibility
        Rectangle {
            anchors.centerIn: parent
            width: 64; height: 64
            color: "#f1c40f"
            radius: 8
        }
        Rectangle {
            x: 16; y: 16
            width: 32; height: 32
            color: "#ffffff"
            radius: 4
        }
        Rectangle {
            anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.rightMargin: 16; anchors.bottomMargin: 16
            width: 48; height: 48
            color: "#e67e22"
            radius: 6
        }
        // Vertical bars for chromatic aberration detection
        Rectangle {
            x: parent.width * 0.25 - 3; y: 0
            width: 6; height: parent.height
            color: "#000000"
        }
        Rectangle {
            x: parent.width * 0.75 - 3; y: 0
            width: 6; height: parent.height
            color: "#000000"
        }
        // Right-edge contrast target: large bezel refraction/edge material
        // should stay visible along straight edges even when the corner radius
        // is much smaller than the bezel width.
        Rectangle {
            x: 214; y: 72
            width: 8; height: 112
            color: "#ffffff"
        }
        Rectangle {
            x: 230; y: 72
            width: 8; height: 112
            color: "#000000"
        }
        Grid {
            anchors.top: parent.top
            anchors.right: parent.right
            columns: 8
            visible: root.cornerProbeVisible

            Repeater {
                model: 64

                Rectangle {
                    required property int index
                    width: 8
                    height: 8
                    color: ((index % 8) + Math.floor(index / 8)) % 2 === 0
                        ? "#ffffff"
                        : "#000000"
                }
            }
        }
    }

    GlassEffect {
        id: glassEffect
        objectName: "glassEffect"
        anchors.fill: parent
        source: backdrop

        transform: Scale {
            id: glassScale
            origin.x: glassEffect.width * 0.5
            origin.y: glassEffect.height * 0.5
        }
    }
}
