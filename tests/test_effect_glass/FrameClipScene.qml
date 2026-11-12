// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import TestGlass

Item {
    id: root
    width: 240
    height: 200
    property real contentScale: 1
    property alias clientSourceRect: clientClip.sourceRect
    readonly property var geometry: ({ rect: Qt.rect(0, 0, width, height), radius: 80 })

    Item {
        width: root.width
        height: 40
        Rectangle {
            id: titleSource
            anchors.fill: parent
            color: "red"
            layer.enabled: true
            opacity: 0
        }
        FrameClip {
            source: titleSource
            geometry: root.geometry
        }
    }

    Item {
        y: 40
        width: root.width * root.contentScale
        height: (root.height - 40) * root.contentScale
        scale: 1 / root.contentScale
        transformOrigin: Item.TopLeft
        Rectangle {
            id: clientSource
            width: 400 * root.contentScale
            height: 260 * root.contentScale
            color: "blue"
            layer.enabled: true
            layer.smooth: true
            opacity: 0
            Rectangle {
                x: 40 * root.contentScale
                y: 20 * root.contentScale
                width: root.width * root.contentScale
                height: (root.height - 40) * root.contentScale
                color: "lime"
            }
        }
        FrameClip {
            id: clientClip
            source: clientSource
            geometry: root.geometry
            geometryScale: root.contentScale
            geometryOffset: Qt.point(0, -40 * root.contentScale)
            sourceRect: Qt.rect(40 * root.contentScale, 20 * root.contentScale,
                                width, height)
        }
    }
}
