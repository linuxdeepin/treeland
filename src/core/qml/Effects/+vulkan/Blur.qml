// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Rectangle {
    id: root

    color: Qt.rgba(0.0, 0.0, 0.0, 0.0)

    property bool radiusEnabled: radius > 0
    property rect frameRect: Qt.rect(0, 0, width, height)
    property int blurMax: WaylandServer.config.blurStrength
    property bool blurEnabled: blurMax > 0 && blurAmount > 0
    property real blurAmount: WaylandServer.config.blurAmount
    property real multiplier: WaylandServer.config.blurMultiplier
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: WaylandServer.config.glassSaturation
    property real glassSpecular: WaylandServer.config.glassSpecular
    property real glassTint: 0.0
    property bool glassEnabled: WaylandServer.config.glassEnabled
    property Item effectContent: effectContentItem
    property bool contentOffscreen: false
    property bool effectEnabled: true
    readonly property Item content: contentContainer
    property alias offscreen: root.contentOffscreen
    default property alias data: contentContainer.data

    Item {
        id: contentContainer
        anchors.fill: parent
    }

    // frameRect can be offset from and larger than this item, so the fill is
    // clipped to the item instead of covering it unconditionally.
    Item {
        id: effectContentItem
        anchors.fill: parent
        clip: true
        visible: root.effectEnabled && !root.contentOffscreen
        z: -1

        Rectangle {
            x: root.frameRect.x
            y: root.frameRect.y
            width: root.frameRect.width
            height: root.frameRect.height
            radius: root.radiusEnabled ? root.radius : 0
            color: Qt.rgba(1.0, 1.0, 1.0, 0.15)
        }
    }

    z: parent.z ? parent.z - 1 : -1
    anchors.fill: parent
}
