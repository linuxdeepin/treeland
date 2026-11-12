// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    required property Item source
    required property var geometry
    property real geometryScale: 1
    property point geometryOffset: Qt.point(0, 0)
    property rect sourceRect: Qt.rect(0, 0, source.width, source.height)

    anchors.fill: parent
    clip: true

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: 0
            fillItem: root.source
            fillTransform: Qt.matrix4x4(
                root.width / (root.sourceRect.width > 0 ? root.sourceRect.width : 1), 0, 0,
                -root.sourceRect.x * root.width / (root.sourceRect.width > 0 ? root.sourceRect.width : 1),
                0, root.height / (root.sourceRect.height > 0 ? root.sourceRect.height : 1), 0,
                -root.sourceRect.y * root.height / (root.sourceRect.height > 0 ? root.sourceRect.height : 1),
                0, 0, 1, 0,
                0, 0, 0, 1)
            PathRectangle {
                x: root.geometry.rect.x * root.geometryScale + root.geometryOffset.x
                y: root.geometry.rect.y * root.geometryScale + root.geometryOffset.y
                width: root.geometry.rect.width * root.geometryScale
                height: root.geometry.rect.height * root.geometryScale
                radius: root.geometry.radius * root.geometryScale
            }
        }
    }
}
