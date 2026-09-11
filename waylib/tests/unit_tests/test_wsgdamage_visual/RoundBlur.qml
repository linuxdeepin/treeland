// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Waylib.Server

// Same structure as src/core/qml/Effects/Blur.qml (no extra overlay
// item) plus the color overlay from src/plugins/lockscreen/qml/RoundBlur.qml.
// Helper.config is not available here; blur amounts match the lockscreen
// RoundBlur defaults (blurMax: 64, blur fully on).
RenderBufferBlitter {
    id: blitter
    smooth: true
    z: parent && parent.z ? parent.z - 1 : -1

    property color color: Qt.rgba(1, 1, 1, 0.1)
    property real radius: 0
    readonly property bool radiusEnabled: radius > 0

    Item {
        anchors.fill: parent

        MultiEffect {
            id: blur
            anchors.fill: parent
            layer.enabled: blitter.radiusEnabled
            smooth: blitter.radiusEnabled
            opacity: blitter.radiusEnabled ? 0 : 1
            source: blitter.content
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            saturation: 0.2
            brightness: 0
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

    Rectangle {
        anchors.fill: parent
        radius: blitter.radius
        color: blitter.color
    }
}
